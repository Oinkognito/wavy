# **Wavy UI**

> [!IMPORTANT]
> 
> This is an isolated codebase of Wavy!!!
> 
> The UI is still in its infancy and will have very **BREAKING** changes.
> 

This will contain the UI implementation of Wavy as a whole using **libquwrof**.

> [!NOTE]
> 
> All global resources are to be placed in `resources/resources.qrc`
> 

## Dependencies 

Refer to [libquwrof](https://github.com/oinkognito/wavy/blob/main/libquwrof/README.md) for dependencies required for Wavy-UI! 

## Building

To build both **libquwrof** and Wavy-UI:

```bash 
cmake -S . -B build 
cmake --build build/
```

To run the binary:

```bash 
./build/Wavy-UI
```
