### **SECURITY**  

# Security Policy  

## **Local Network Security Considerations**  
Wavy is designed as a **local network solution** for audio streaming and sharing. Since it primarily operates within a trusted network, the **server utilizes self-signed SSL/TLS certificates** to enable secure communication.  

> [!WARNING]
> If Wavy is exposed to the public internet, self-signed certificates **do not guarantee** security against MITM (Man-in-the-Middle) attacks 
> or unauthorized interception. 
> 
> Users should ensure proper firewall rules and network segmentation if using Wavy outside a private network.
> 
> This project is **NOT** designed to be an internet solution for audio transfer and playback!
> 

## **Secure Coding Practices**  
- The codebase follows secure programming principles to mitigate common vulnerabilities such as buffer overflows, memory corruption, and race conditions.  
- Regular **CodeQL** analysis is performed to detect and fix security flaws before release.  
- Dependencies are routinely checked for vulnerabilities, and patches are applied when necessary.  

## **Responsible Disclosure**  
If you discover any **potential security issues** in Wavy, **DO NOT** create a public issue on GitHub, as this may expose vulnerabilities before they are patched.  

Instead, **immediately report security concerns via email**:  
**[sid9.karanam@gmail.com]**  

## **No Absolute Security Guarantees**  
Wavy is still **under active development**, and while security is a high priority, no absolute guarantees can be made at this stage. **Use at your own discretion**, especially in environments where security is critical.  

Stay updated with project changes!  
