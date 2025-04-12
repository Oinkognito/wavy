# FetcherFactory (Plugin Manager for TSFetch logic)

In a nutshell, this is the plugin manager that attempts to load shared object (.so) [which are just dynamically loaded libraries] safely into
`Wavy-Client` (the component that is responsible for fetching the transport streams and playing the audio)

## Why is this required?

To accomodate for future more optimized ways to fetching transport streams. The `Wavy-Server` component aims to be *just* a smart storage facility with minimal validation and computing tasks (to avoid computational overhead). 

In fact, we want the extra mile of ensuring that the server component **NEVER** has to required FFmpeg as a dependency to avoid additional downloads or extra size of your docker container, etc. A TLDR; The server is only a friend that you grab candy from. Nothing else.

So the onus of fetching which transport stream, playlist at a specific bitrate lies on `Wavy-Client`. This calls for a lot of possible ways of doing this.

This is an interesting problem that requires a **MODULAR** solution. Take the example of `vlc` media player. When you try to play a network stream, by default way it tries to **preemptive fetch** the next transport stream. Now what does that mean? Let us take an example to understand this easily.

**Preemptive Fetch:**

As the name suggests, before the current transport stream finishes playback, the client will try to fetch the next (presumably with ABR) transport stream from the server. 

This is a *minimal* and *neat* solution for optimal playback, it eliminates having a lot of audio data in your memory, no pause between streams, can be integrated with ABR easily, and a key point of this is that if there is some corrupted data received by the client, the client only has to request resend of stream 'x' instead of the entire playlist.

Some disadvantages to this method is the fact that you **DO NOT** have the entire song in your memory, so for every stream transfer you need to have stable network which can be a tall ask in some areas.

> [!NOTE]
> 
> Just a side note, these are **MY** observations 
> and independent research on the matter, so the terminologies
> that I maybe using is **NOT** the official lingo. 
> 
> The goal for this anyway is to help you understand why this is needed.
> 

Another example is of `mpv`. The way it fetches is **Preemptive Aggressive** approach. By aggresive what we mean is that it requests for streams from the server in a **VERY SMALL** amount of time (which can be viewed by running any network stream it is very noticeable).

So when we try to play the network stream in `mpv`, it will fetch streams **aggressively** all-the-while playing the current audio segment without **ANY** pause in playback whatsoever.

Why is this preemptive? It's because `mpv` does not wait for the streams to be loaded in memory, it has immediate playback and simultaneous stream fetch in the background for minimal idle time for the player.

Jury on this approach is that this is quite optimal in most networking scenarios; whether you have poor, unstable or great, stable networking status, this ensures the "grab the gold while you can" policy. This works with the current networking scenario and makes the most of it without any surety or reliance on the future conditions.

What this entails are the advantages of **MINIMAL IDLE** time, can allow for playback for much longer (even fully) due to aggressive caching, no pause between segments, and can be integrated with ABR but this is *quite pointless*.

What do we mean by pointless? ABR only works for **segment-by-segment** fetch for maximum optimal playback. Let us take an example: if we **aggressively** fetch a 128kbps audio playlist and have some ABR implementation, the time base of playback vs time base of fetch time is different.

```text 
---  START 0:00
---> AGGRESSIVE FETCH (128kbps) [EACH SEGMENT is 10s]
---  PLAYBACK STARTS
...
===> FETCH segment0 at 0s                              | ===> PLAYBACK of segment0 at 0s
===> FETCH segment1 at 0s                              | ===> PLAYBACK of segment0 at 0s
...                                                    | ...
===> FETCH segmentN at 0+ds (d is variable offset)     | ...
===> UNKNOWN OP/N                                      | ===> PLAYBACK of segment0 at 10s 
===> FETCH segmentM at 10s                             | ===> PLAYBACK of segment1 at 10s
```

In case this is not clear enough, here is another timing diagram example of the time base misalignment:

```md
                        AGGRESSIVE FETCH (128kbps)
                        [EACH SEGMENT PLAYBACK   = 10s]
                        [EACH SEGMENT FETCH TIME = 1s ]

| Time (s)       |   0     |   1     |   2     |   3     |   4     |   5     |   6     |   7     |   8     |   9     |   10    |   11    |   12    |   13    |
|----------------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|
| Fetch Segment  |    0    |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |    10   |    11   |    12   |    13   |
| Playback       | Segment0| Segment0| Segment0| Segment0| Segment0| Segment0| Segment0| Segment0| Segment0| Segment0| Segment1| Segment1| Segment1| Segment1|
```

Hopefully this minimal timing diagram explains the problem with trying to implement ABR in **aggressive** fetch. The timing of playback of a particular segment VS timing of fetching 'n'th segment is completely different.

Why does this matter? Because the concept of ABR is now pointless. Adaptive bitrate streaming is supposed to be **real-time** adaptive streaming, but since we are fetching segments so **aggressively**, this is no longer real-time but rather just a very preemptive fetch that does not align with the window size of an ideal ABR implementation.

[Window Size]: Refers to a period of time where fetching the segment vs playing a segment is optimal for ABR (ex: 1s before a segment ends, we fetch the next segment; similar to vlc essentially)

In context of understanding it with just a plain **Preemptive** approach like we saw in `vlc`, if `vlc` preemptively fetched every segment with an increment of 0.1s, it would also cause this issue and can be labelled as **aggressive** fetching. The window size is lost in this scenario and ABR is now meaningless.

As a result, (in my opinion) ABR is **meaningless** to implement in an **aggressive** fetch scenario. This is further justified by looking at what streams does `mpv` fetch in this method --> only the highest bitrate stream is always fetched: **NO EXCEPTION**.

This is the "grab the gold while you can" scenario. Without hesitation or wait, we try to grab the **BEST** possible result even despite the obvious downfalls of more transmission time, more data loaded into memory and so on, **REGARDLESS** of the networking condition.

Some other fallacies of this is the fact that if we receive corrupt data, we need to fetch the entire playlist again (so excessive retransmissions) and some delay in transmission depending on the bitrate chosen.

Hence, yes this is **DEEPLY** required. For comparison between different methods to giving users the priviledge to choose their poison, this just makes sense to have in Wavy.

Some may disagree and conclude that `mpv` or `vlc`'s implementation is good enough. It may be. The only way to test them **BOTH** out and find the best one is through this.

## What it does 

It is quite simple really; below is the logical flow that should make it clear as to what is happening.

#### Logical Flow:

1. **Plugin Path and Server Setup:**
   - The `FetcherFactory::create()` method takes two parameters:
     - **plugin_path**: The path to the shared object (`.so`) file containing the plugin.
     - **server**: The server address or information passed to the fetcher for initializing the transport stream fetcher.

2. **Loading the Plugin:**
   - The function begins by loading the specified plugin shared object file using `dlopen()`. This function attempts to dynamically load the shared library from the provided path.
   - If the plugin cannot be loaded (due to incorrect path or missing library), the function throws an error with a message describing the problem, using `dlerror()` to retrieve the error message from the dynamic loader.

3. **Resolving the Plugin’s Symbols:**
   - Once the plugin is loaded, the factory attempts to find the symbol for the `create_fetcher_with_arg` function using `dlsym()`. This function is expected to be part of the plugin and is responsible for creating a fetcher instance.
   - If the symbol is not found, the factory closes the plugin handle and throws an error. This ensures that the system doesn't try to use an incomplete or incorrect plugin.

4. **Creating the Fetcher Instance:**
   - The factory calls the `create_fetcher_with_arg` function (retrieved symbol) with the server address passed in the parameters. This function should return a pointer to an instance of the `ISegmentFetcher` interface.
   - If the fetcher pointer is null, the factory unloads the plugin and throws an error, indicating the fetcher creation failed.

5. **Returning the Fetcher Instance:**
   - After successfully creating the fetcher, the factory wraps the pointer to the `ISegmentFetcher` instance in a `std::unique_ptr`. The unique pointer is equipped with a custom deleter, which ensures that the plugin is unloaded and the fetcher instance is destroyed properly when the `unique_ptr` goes out of scope.
   - This design ensures that the plugin is automatically unloaded from memory when it's no longer needed, preventing memory leaks or dangling pointers.

6. **Memory Management and Cleanup:**
   - When the fetcher instance is destroyed (i.e., the `std::unique_ptr` is destroyed), the custom deleter is invoked. This calls `dlclose()` to unload the plugin from memory and deletes the fetcher instance, ensuring proper cleanup.
