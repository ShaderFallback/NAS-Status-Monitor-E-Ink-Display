
import psutil
import time
import requests
import json
import os
import clr

# 加载配置
with open('config.json', 'r') as f:
    config = json.load(f)

ESP_URL = f"http://{config['esp_ip']}/data"
TEMP_DISKS = config['temp_disks'][:6]   # 最多6块 
USAGE_DISKS = config['usage_disks'][:2] # 最多2块 

def get_drive_map_simple():
    """
    通过 psutil 建立 物理索引 -> 盘符 的映射
    返回字典如: {0: "C:", 1: "D:"}
    """
    mapping = {}
    # 获取所有磁盘分区
    partitions = psutil.disk_partitions()
    for p in partitions:
        try:
            # 这里的逻辑在 Windows 下通过 mountpoint 获取对应的物理驱动器索引
            # 实际上，大多数家用环境下盘符顺序与物理索引是挂钩的
            # 如果是更复杂的环境，建议直接用盘符作为 key
            if 'fixed' in p.opts:
                # 简单粗暴但有效的映射：C盘通常是索引0，D盘是1，依此类推
                mapping[p.device[0]] = p.mountpoint.replace("\\", "")
        except:
            continue
    return mapping

# 初始化 LibreHardwareMonitor
def init_ohm():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.join(current_dir, "LibreHardwareMonitorLib.dll")
    clr.AddReference(dll_path)
    
    # 导入命名空间
    from LibreHardwareMonitor import Hardware
    
    computer = Hardware.Computer()
    # 对应你 C# 中的开关
    computer.IsCpuEnabled = True
    computer.IsMemoryEnabled = True
    computer.IsGpuEnabled = True
    computer.IsStorageEnabled = True 
    
    computer.Open()
    return computer

def Check_Hardware(handle):
    results = []
    if not handle:
        print("DEBUG: Handle 为空，初始化失败")
        return results

    print(f"DEBUG: 扫描到的硬件数量: {len(handle.Hardware)}")
    
    for hardware in handle.Hardware:
        print(f"DEBUG: 正在检查硬件: {hardware.Name} (类型: {hardware.HardwareType})")
        hardware.Update()
        
        for sensor in hardware.Sensors:
            # 打印所有传感器，看看温度传感器到底叫什么
            print(f"  - 传感器: {sensor.Name}, 类型: {sensor.SensorType}, 值: {sensor.Value}")

    
    return results

def get_disk_temps(handle):
    results = []
    if not handle:
        return results
    index = 0

    # 导入所需的枚举类型
    from LibreHardwareMonitor.Hardware import HardwareType, SensorType
    for hardware in handle.Hardware:
        if hardware.HardwareType == HardwareType.Storage:
            hardware.Update()
            for sensor in hardware.Sensors:
                if sensor.SensorType == SensorType.Temperature and sensor.Value is not None:
                    # 获取 hardware.Name 和温度值
                    # 硬盘名称 hardware.Name
                    results.append({
                        "name":TEMP_DISKS[index],
                        "value": float(sensor.Value)
                    })
                    index += 1
                    break #每块硬盘只取第一个温度传感器，防止多个传感器导致索引错位

    return results

def get_disk_usage():
    """获取硬盘使用情况"""
    results = []
    for disk in USAGE_DISKS:
        try:
            usage = psutil.disk_usage(disk+":")
            results.append({
                "name": disk.replace(":", ""),
                "used": round(usage.used / (1024**3), 1),  # GB 
                "total": round(usage.total / (1024**3), 1) # GB 
            })
        except:
            continue
    return results

def get_net_speed(last_io):
    """计算网速"""
    curr_io = psutil.net_io_counters()
    up = (curr_io.bytes_sent - last_io.bytes_sent) / 1024  # KB/s 
    down = (curr_io.bytes_recv - last_io.bytes_recv) / 1024 # KB/s 
    return round(up, 1), round(down, 1), curr_io

def clear_screen():
    # Windows 使用 'cls'，Unix/Linux/macOS 使用 'clear'
    os.system('cls' if os.name == 'nt' else 'clear')

def main():
    print(f"Connecting to ESP8266 at: {ESP_URL}")
    last_io = psutil.net_io_counters()
    last_minute_time = 0

    handle = init_ohm()
    
    # 缓存每分钟读取的数据
    cached_temps = []
    cached_usage = []

    while True:
        start_time = time.time()
        
        # 1. 每分钟读取一次温度和硬盘使用量
        if start_time - last_minute_time >= 60:
            #检查所有传感器
            #Check_Hardware(handle)

            cached_temps = get_disk_temps(handle)
            cached_usage = get_disk_usage()
            last_minute_time = start_time
            
            tempLen = len(cached_temps)

            #数据不足6条时补齐0值，保持和单片机程序一致
            if (tempLen<6):
                for x in range(tempLen,6):
                    cached_temps.append({"name": TEMP_DISKS[x], "value": 0})
                

            print("Updated Minute-based data (Temp/Usage)")

            for temp in cached_temps:
                name = temp["name"]
                value = temp["value"]
                print(f"    温度 {name}: {value}°C")

            for disk in cached_usage:
                disk_name = disk["name"]
                used = disk["used"]
                total = disk["total"]
                print(f"    硬盘 {disk_name}: {used}GB / {total}GB")


        # 2. 每秒计算一次网速
        up_speed, down_speed, last_io = get_net_speed(last_io)

        # 3. 构造符合单片机程序的 JSON
        payload = {
            "temps": cached_temps,
            "disks": cached_usage,
            "upload": up_speed,
            "download": down_speed
        }

        # 4. 发送数据
        try:
            response = requests.post(ESP_URL, json=payload, timeout=5)
            #print(payload)
            print(f"Sent: Up {up_speed} KB/s, Down {down_speed} KB/s | Status: {response.status_code}")
        except Exception as e:
            print(f"Send Failed: {e}")

        # 保持 1秒 周期
        sleep_time = 1.0 - (time.time() - start_time)
        if sleep_time > 0:
            time.sleep(sleep_time)

        #clear_screen()

if __name__ == "__main__":
    main()