# 📡 NAS 状态监控器 - 墨水屏版（开源）

## ✨ 特性

1. 支持 **6 个硬盘温度显示**（每分钟刷新）  
   - 若不显示图标，可扩展至 **9 个温度显示**

2. 支持 **2 个硬盘容量显示**（每分钟刷新）

3. 支持 **60 秒网络吞吐曲线显示**  
   - 虚线：上传速度  
   - 实线：下载速度  
   - 数据自适应 `Umax / Dmax`  
   - 网格最大值为当前区间峰值  
   - 超过 `1000 kb/s` 自动显示为 `Mb/s`  
   - 网络数据 **每秒更新**（屏幕每分钟刷新一次）

---

## ⚠️ 注意事项

1. 请自行修改 Arduino 代码第 10 行的 **WiFi 名称和密码**

2. 使用 **ESP8266 驱动**  
   墨水屏库采用 **GxEPD2**（请在 Arduino 中搜索并安装）

3. 上位机默认支持 **Windows**  
   如需自行修改，请遵循 `example.json` 数据格式

4. 可使用 `#Check_Hardware(handle)#` 检查所有传感器

---

## 💡 使用建议

建议通过任务管理器设置为 **开机后台自动运行**

---

## 📢 关注

欢迎关注以下平台账号：  
**B站 / YouTube / 小红书：VoyagerDIY**

---

# 🌍 English Version

# 📡 NAS Status Monitor - E-Ink Display Edition (Open Source)

## ✨ Features

1. Supports **temperature display for up to 6 drives** (updated every minute)  
   - Can be extended to **9 temperatures** if icons are disabled  

2. Supports **capacity display for 2 drives** (updated every minute)

3. Displays **network throughput over the last 60 seconds**  
   - Dashed line: Upload speed  
   - Solid line: Download speed  
   - Auto-scaled based on `Umax / Dmax`  
   - Grid maximum reflects peak value within the range  
   - Speeds above `1000 kb/s` are shown in `Mb/s`  
   - Network data updates **every second** (screen refreshes every minute)

---

## ⚠️ Notes

1. Please modify the **WiFi SSID and password** in line 10 of the Arduino code  

2. Powered by **ESP8266**  
   Uses the **GxEPD2** library (install via Arduino Library Manager)

3. The host software supports **Windows by default**  
   If modifying, follow the `example.json` data format

4. Use `#Check_Hardware(handle)#` to verify all sensors

---

## 💡 Recommendation

It is recommended to configure the program to **run automatically in the background on system startup**

---

## 📢 Follow

Find more content on:  
**Bilibili / YouTube / Xiaohongshu: VoyagerDIY**
