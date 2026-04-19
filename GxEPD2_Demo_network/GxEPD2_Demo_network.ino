#include <GxEPD2_BW.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <math.h>

// ================= WiFi =================
const char* ssid = "****";
const char* password = "****";

// ================= 屏幕 =================
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(
  GxEPD2_420_GDEY042T81(15, 4, 2, 5)
);

// ================= HTTP =================
ESP8266WebServer server(80);

// ================= 数据 =================

#define MAX_TEMP 6

struct TempItem
{
  String name;
  float value;
};

TempItem temps[MAX_TEMP];
int tempCount = 0;

#define MAX_DISK 4

struct DiskItem
{
  String name;
  float used;
  float total;
};

DiskItem disks[MAX_DISK];
int diskCount = 0;

float uploadSpeed=0;
float downloadSpeed=0;

// ================= 历史 =================
#define HISTORY_LEN 60
float upHist[HISTORY_LEN];
float downHist[HISTORY_LEN];
int histIndex = 0;

// ================= 刷新 =================
unsigned long lastDraw = 0;
unsigned long lastSample = 0;
int refreshInterval = 60000;

// ================= 图表区域（铺满） =================
#define GRAPH_X 0
#define GRAPH_Y 175
#define GRAPH_W 400
#define GRAPH_H 120

// ================= 初始化假数据 =================
void initHistory()
{
  for (int i = 0; i < HISTORY_LEN; i++)
  {
    float base = 300 + 80 * sin(i * 0.2);
    downHist[i] = base + random(-40, 40);
    upHist[i]   = base * 0.5 + random(-20, 20);
  }
}
// ================= 工具函数：格式化网速 =================
//自动转换 KB 到 MB
String formatSpeed(float kbs) {
  if (kbs >= 1024.0) {
    return String(kbs / 1024.0, 2) + " Mb/s";
  } else {
    return String(kbs, 0) + " kb/s";
  }
}
// ================= 数据推进 =================
void pushHistory(float up, float down)
{
  upHist[histIndex] = up;
  downHist[histIndex] = down;
  histIndex = (histIndex + 1) % HISTORY_LEN;
}

// ================= 最大值 =================
float getMaxValue()
{
  float m = 10;
  for (int i = 0; i < HISTORY_LEN; i++)
  {
    if (upHist[i] > m) m = upHist[i];
    if (downHist[i] > m) m = downHist[i];
  }
  return m * 1.1; // 留一点顶部空间
}
float getMaxUpload()
{
  float m = 0;
  for (int i = 0; i < HISTORY_LEN; i++)
  {
    if (upHist[i] > m) m = upHist[i];
  }
  return m;
}

float getMaxDownload()
{
  float m = 0;
  for (int i = 0; i < HISTORY_LEN; i++)
  {
    if (downHist[i] > m) m = downHist[i];
  }
  return m;
}

// ================= HTTP =================
void handleData()
{
  if (server.hasArg("plain"))
  {
    // V7 统一使用 JsonDocument，无需指定大小
    JsonDocument doc; 

    if (!deserializeJson(doc, server.arg("plain")))
    {
      // ===== 温度 =====
      // 使用 .is<JsonArray>() 替代 containsKey
      if (doc["temps"].is<JsonArray>()) 
      {
        JsonArray arr = doc["temps"];
        tempCount = 0;

        for (JsonObject obj : arr)
        {
          if (tempCount >= MAX_TEMP) break;
          temps[tempCount].name  = obj["name"].as<String>();
          temps[tempCount].value = obj["value"] | 0.0f; // 明确指定为 float 默认值

          tempCount++;
        }
      }

      // ===== 磁盘 =====
      // 同理，使用 .is<JsonArray>() 检查
      if (doc["disks"].is<JsonArray>())
      {
        JsonArray arr = doc["disks"];
        diskCount = 0;

        for (JsonObject obj : arr)
        {
          if (diskCount >= MAX_DISK) break;
          disks[diskCount].name  = obj["name"].as<String>();
          disks[diskCount].used  = obj["used"]  | 0.0f;
          disks[diskCount].total = obj["total"] | 1.0f;

          diskCount++;
        }
      }

      // ===== 网速 =====
      uploadSpeed   = doc["upload"]   | uploadSpeed; 
      downloadSpeed = doc["download"] | downloadSpeed;

      pushHistory(uploadSpeed, downloadSpeed);
    }
  }
  server.send(200, "text/plain", "OK");
}


//图标

void drawComputerIcon(int cx, int cy, int r)
{
  // ===== 外圆 =====
  //display.drawCircle(cx, cy, r, GxEPD_BLACK);
  //display.drawCircle(cx, cy, r - 1, GxEPD_BLACK); // 稍微加粗

  // ===== 屏幕（矩形）=====
  int screenW = r * 1.2;
  int screenH = r * 0.8;

  int sx = cx - screenW / 2;
  int sy = cy - screenH / 2;

  display.drawRect(sx, sy, screenW, screenH, GxEPD_BLACK);

  // 内边框（让屏幕更像UI）
  display.fillRect(sx + 2, sy + 2, screenW - 4, screenH - 4, GxEPD_BLACK);

  // ===== 支架 =====
  int standW = screenW * 0.3;
  int standH = 4;

  int standX = cx - standW / 2;
  int standY = sy + screenH + 2;

  display.fillRect(standX, standY, standW, standH, GxEPD_BLACK);

  // ===== 底座 =====
  int baseW = screenW * 0.6;
  int baseH = 3;

  int baseX = cx - baseW / 2;
  int baseY = standY + standH + 2;

  display.fillRect(baseX, baseY, baseW, baseH, GxEPD_BLACK);
}

// ================= 网格 =================
void drawGrid()
{
  // 横线
  for (int i = 0; i <= 3; i++)
  {
    int y = GRAPH_Y + i * (GRAPH_H / 3);
    display.drawLine(0, y, 400, y, GxEPD_BLACK);
  }

  // 竖线（更密）
  for (int x = 0; x <= 400; x += 40)
  {
    display.drawLine(x, GRAPH_Y, x, GRAPH_Y + GRAPH_H, GxEPD_BLACK);
  }
}

void drawDashedLine(int x1, int y1, int x2, int y2, int dash, int gap)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx * dx + dy * dy);

    float unitX = dx / length;
    float unitY = dy / length;

    float pos = 0;

    while (pos < length)
    {
        float start = pos;
        float end = min(pos + dash, length);

        int sx = x1 + unitX * start;
        int sy = y1 + unitY * start;
        int ex = x1 + unitX * end;
        int ey = y1 + unitY * end;

        display.drawLine(sx, sy, ex, ey, GxEPD_BLACK);

        pos += dash + gap;
    }
}

// ================= 曲线 =================
void drawGraph()
{
  float maxV = getMaxValue();
  int steps = HISTORY_LEN * 2;

  for (int i = 0; i < steps - 1; i++)
  {
    float t1 = (float)i / steps * HISTORY_LEN;
    float t2 = (float)(i + 1) / steps * HISTORY_LEN;

    int idx1 = ((int)t1 + histIndex) % HISTORY_LEN;
    int idx2 = ((int)t2 + histIndex) % HISTORY_LEN;

    float lerp1 = t1 - (int)t1;
    float lerp2 = t2 - (int)t2;

    int next1 = (idx1 + 1) % HISTORY_LEN;
    int next2 = (idx2 + 1) % HISTORY_LEN;

    float down1 = downHist[idx1]*(1-lerp1)+downHist[next1]*lerp1;
    float down2 = downHist[idx2]*(1-lerp2)+downHist[next2]*lerp2;

    float up1 = upHist[idx1]*(1-lerp1)+upHist[next1]*lerp1;
    float up2 = upHist[idx2]*(1-lerp2)+upHist[next2]*lerp2;

    int x1 = (i * GRAPH_W) / steps;
    int x2 = ((i + 1) * GRAPH_W) / steps;

    int y1d = GRAPH_Y + GRAPH_H - (down1 / maxV) * GRAPH_H;
    int y2d = GRAPH_Y + GRAPH_H - (down2 / maxV) * GRAPH_H;

    int y1u = GRAPH_Y + GRAPH_H - (up1 / maxV) * GRAPH_H;
    int y2u = GRAPH_Y + GRAPH_H - (up2 / maxV) * GRAPH_H;

    // 下载（实线）
    display.drawLine(x1, y1d, x2, y2d, GxEPD_BLACK);
    display.drawLine(x1, y1d+1, x2, y2d+1, GxEPD_BLACK);

    // 上传（虚线）
    //if ((i / 1) % 2 == 0)
    //display.drawLine(x1, y1u, x2, y2u, GxEPD_BLACK);
    drawDashedLine(x1, y1u, x2, y2u, 2, 3);
  }
}

// ================= UI =================
void drawUI()
{
  display.setFullWindow();

  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);

    display.drawLine(0, 87, 400, 87, GxEPD_BLACK);

    display.setFont(&FreeMonoBold12pt7b);

    // 左列 (temps[0] ~ temps[2])
    for (int i = 0; i < 3; i++)
    {
        int x = 120;
        int y = 30 + i * 25;

        display.setCursor(x, y);
        display.printf("%s:%d 'C", temps[i].name.c_str(), (int)temps[i].value);
    }

    // 右列 (temps[3] ~ temps[5])
    for (int i = 0; i < 3; i++)
    {
        int x = 260;
        int y = 30 + i * 25;

        display.setCursor(x, y);
        display.printf("%s:%d 'C", temps[i + 3].name.c_str(), (int)temps[i + 3].value);
    }

    display.setFont(&FreeMonoBold9pt7b);
    
    //绘制硬盘显示
    int barX = 30;
    int barY = 110;
    int barW = 150;
    int barH = 10;
    int spacingX = 50;

    for (int i = 0; i < 2; i++)
    {
        int x = barX + i * (barW + spacingX);
        int y = barY;

        float used = disks[i].used;
        float total = disks[i].total;

        float ratio = used / total;
        float available = total - used; 
        if (ratio > 1) 
        {
            ratio = 1;
        }
        display.drawRect(x, y, barW, barH, GxEPD_BLACK);
        display.fillRect(x, y, barW * ratio, barH, GxEPD_BLACK);

        display.setCursor(x, y - 3);
        display.printf("%.0f GB/%.0f GB", available, total);

        display.setCursor(x - 25, y + 8);
        display.printf("%s:",disks[i].name);
    }

    
    //绘制图标
    drawComputerIcon(70, 30, 35);
    display.setCursor(50, 70);
    display.print("TEMP");

    // ===== 网速=====
    display.setCursor(15, GRAPH_Y - 25);
    display.print("Up  :");
    display.print(formatSpeed(uploadSpeed));
    display.setCursor(225, GRAPH_Y - 25);
    display.print("Down:");
    display.print(formatSpeed(downloadSpeed));

    // ===== 网速最大值=====
    float upMax = getMaxUpload();
    float downMax = getMaxDownload();

    // 最大值（60秒）
    display.setCursor(15, (GRAPH_Y - 7));
    display.print("Umax:");
    display.print(formatSpeed(upMax));

    display.setCursor(225, GRAPH_Y - 7);
    display.print("Dmax:");
    display.print(formatSpeed(downMax));
        
    drawGrid();
    drawGraph();

    // 时间轴
    display.setCursor(5, GRAPH_Y + GRAPH_H - 3);
    display.print("60s");

    display.setCursor(380, GRAPH_Y + GRAPH_H -3);
    display.print("0");

  } while (display.nextPage());

  lastDraw = millis();
}

// ================= 初始化 =================
void setup()
{
  display.init();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);

  display.firstPage();
  do
  {
    display.setCursor(120, 150);
    display.printf("Wifi Connect......");
  }while (display.nextPage());


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
    delay(500);

  IPAddress ip = WiFi.localIP();

  display.firstPage();
  do
  {
    display.setCursor(120, 150);
    display.printf("ip: %d.%d.%d.%d",ip[0], ip[1], ip[2], ip[3]);
  }while (display.nextPage());
  
  server.on("/data", HTTP_POST, handleData);
  server.begin();

  randomSeed(analogRead(A0));
  initHistory();

  drawUI();
}

// ================= 循环 =================
void loop()
{
  server.handleClient();

  if (millis() - lastSample > 1000)
  {
    lastSample = millis();
    pushHistory(uploadSpeed, downloadSpeed);
  }

  if (millis() - lastDraw > refreshInterval)
  {
    drawUI();
  }
}