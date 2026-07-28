# Security policy

## Supported versions

| Version | Supported |
|---|---|
| 1.x | Yes |
| 0.6.x | No |
| 0.5.x | No |
| 0.4.x | No |
| 0.3.x | No |
| 0.2.x | No |
| 0.1.x | No |

Support means that reports are evaluated and fixes may be issued. It does not
imply a stable binary ABI, exchange certification, or suitability for production
trading. Security fixes issued in 1.x follow the documented source, CMake, and
CLI compatibility policy. An unavoidable break to those surfaces requires a
new package major version and an explicit migration notice.

## Reporting a vulnerability

Do not open a public issue for a security-sensitive finding. Use GitHub's
private vulnerability reporting for this repository when available. If that
entry point is unavailable, contact the repository owner through a private
channel listed on their GitHub profile.

Include the affected version or commit, compiler and platform, a minimal
reproducer, expected impact, and whether malformed or untrusted bytes are
required. Do not attach credentials, proprietary captures, licensed exchange
data, or other secrets.

You should receive an acknowledgement within seven days. Please allow time for
reproduction, a coordinated fix, and release preparation before disclosure.
