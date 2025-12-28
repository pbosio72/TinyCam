## 🔍 Troubleshooting

### Code Upload Issues (Debian/Linux specific)

**Problem:** Code verification and upload must happen together, otherwise it freezes.

**Solution:**
- Open a fresh Arduino IDE
- Press arrow button to compile and upload in one action

### Board Version Issues

**Problem:** Board did not work with ESP32 core version 3.xx

**Solution:**
- Downgrade to version **2.0.17**
- The freeze error appeared as a quote character error

### Common Issues

| Problem | Solution |
|---------|----------|
| Quote compilation error | Install python3-serial: `sudo apt-get install python3-serial` |
| OLED display not working | Verify I2C address (0x3C), check SDA/SCL connections, verify 5V power |
| Stream not visible on mobile | Try Chrome browser, access `http://192.168.4.1/stream` directly |
| Flash LED not turning on | LED shares pin with camera, increase brightness with slider |
| Cannot upload sketch | Verify GPIO 0 connected to GND during upload, check USB driver |
| High resolution streaming slow | Use VGA for best performance, SVGA is good compromise |

---
