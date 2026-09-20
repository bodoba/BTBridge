# BTBridge

Enbable sharing of Bluetooth Keyboard and Mouse between PC by bridging them to a USB HID devive.

The idea is to pair Bluetooth keyboard and mouse witha low end Linux device (in my case an old Orange Pi), which itself represents a HID compisite device that can be switched between PCs by a standard USB switch:

```text
   +--------+                 +----------+
   | Mouse  |                 | Keyboard |
   +--------+                 +----------+
          \                     /
           \                   /
            \   Bluetooth     /
             \               /
              v             v
          +---------------------+
          |        Linux        |
          +---------------------+
                     |
                     | USB
                     v
          +---------------------+
          |      USB Switch     |
          +---------------------+
               /           \
              /             \
             /               \
            v                 v
     +-------------+   +-------------+
     |    PC #1    |   |    PC #2    |
     +-------------+   +-------------+
```
