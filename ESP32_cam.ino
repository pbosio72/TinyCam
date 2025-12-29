#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===========================================
// HOTSPOT CREDENTIALS
// ===========================================
const char* ssid = "xxxxxxxx";      // Hotspot name
const char* password = "xxxxxxxx";   // Password (minimum 8 characters)

// Pin definitions for AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define LED_GPIO_NUM       4  // Flash LED pin

// I2C pins for OLED display
#define I2C_SDA 14
#define I2C_SCL 15

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

httpd_handle_t camera_httpd = NULL;
bool flashOn = false;
int flashBrightness = 0;

// PWM channel for LED control
#define PWM_CHANNEL 7
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// Image orientation flags
bool verticalFlip = false;
bool horizontalMirror = false;

// Current resolution
framesize_t currentResolution = FRAMESIZE_VGA;

// Update OLED display
void updateDisplay(String line1, String line2 = "", String line3 = "", String line4 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println(line1);
  
  if (line2.length() > 0) {
    display.setCursor(0, 16);
    display.println(line2);
  }
  
  if (line3.length() > 0) {
    display.setCursor(0, 32);
    display.println(line3);
  }
  
  if (line4.length() > 0) {
    display.setCursor(0, 48);
    display.println(line4);
  }
  
  display.display();
}

// Apply image orientation settings
void applyOrientation() {
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, verticalFlip ? 1 : 0);
    s->set_hmirror(s, horizontalMirror ? 1 : 0);
    Serial.print("Orientation updated - vflip: ");
    Serial.print(verticalFlip);
    Serial.print(" hmirror: ");
    Serial.println(horizontalMirror);
  }
}

// Apply resolution change
void applyResolution(framesize_t newRes) {
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_framesize(s, newRes);
    currentResolution = newRes;
    Serial.print("Resolution changed to: ");
    Serial.println(newRes);
  }
}

// Handler for image capture (streaming)
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// Handler for photo capture and download
static esp_err_t photo_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Photo capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // Generate filename with timestamp
  char filename[50];
  unsigned long timestamp = millis();
  snprintf(filename, sizeof(filename), "attachment; filename=photo_%lu.jpg", timestamp);

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", filename);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  Serial.println("Photo captured and sent");
  return res;
}

// Handler for flash LED ON
static esp_err_t flash_on_handler(httpd_req_t *req) {
  flashBrightness = 255;
  ledcWrite(PWM_CHANNEL, flashBrightness);
  flashOn = true;
  Serial.println("Flash ON");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

// Handler for flash LED OFF
static esp_err_t flash_off_handler(httpd_req_t *req) {
  flashBrightness = 0;
  ledcWrite(PWM_CHANNEL, flashBrightness);
  flashOn = false;
  Serial.println("Flash OFF");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

// Handler for brightness adjustment
static esp_err_t brightness_handler(httpd_req_t *req) {
  char buf[100];
  int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
  
  if (ret == ESP_OK) {
    char param[32];
    if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
      flashBrightness = atoi(param);
      ledcWrite(PWM_CHANNEL, flashBrightness);
      Serial.print("Brightness: ");
      Serial.println(flashBrightness);
    }
  }
  
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

// Handler for image rotation (180 degrees)
static esp_err_t rotate_handler(httpd_req_t *req) {
  char buf[100];
  int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
  
  if (ret == ESP_OK) {
    char param[32];
    if (httpd_query_key_value(buf, "mode", param, sizeof(param)) == ESP_OK) {
      if (strcmp(param, "normal") == 0) {
        verticalFlip = false;
        horizontalMirror = false;
        Serial.println("Orientation: Normal");
      } else if (strcmp(param, "180") == 0) {
        verticalFlip = true;
        horizontalMirror = true;
        Serial.println("Orientation: 180 degrees");
      } else if (strcmp(param, "vflip") == 0) {
        verticalFlip = true;
        horizontalMirror = false;
        Serial.println("Orientation: Vertical flip");
      } else if (strcmp(param, "hmirror") == 0) {
        verticalFlip = false;
        horizontalMirror = true;
        Serial.println("Orientation: Horizontal mirror");
      }
      applyOrientation();
    }
  }
  
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

// Handler for resolution change
static esp_err_t resolution_handler(httpd_req_t *req) {
  char buf[100];
  int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
  
  if (ret == ESP_OK) {
    char param[32];
    if (httpd_query_key_value(buf, "res", param, sizeof(param)) == ESP_OK) {
      framesize_t newRes = FRAMESIZE_VGA;
      
      if (strcmp(param, "qvga") == 0) {
        newRes = FRAMESIZE_QVGA;  // 320x240
      } else if (strcmp(param, "vga") == 0) {
        newRes = FRAMESIZE_VGA;   // 640x480
      } else if (strcmp(param, "svga") == 0) {
        newRes = FRAMESIZE_SVGA;  // 800x600
      } else if (strcmp(param, "xga") == 0) {
        newRes = FRAMESIZE_XGA;   // 1024x768
      } else if (strcmp(param, "sxga") == 0) {
        newRes = FRAMESIZE_SXGA;  // 1280x1024
      } else if (strcmp(param, "uxga") == 0) {
        newRes = FRAMESIZE_UXGA;  // 1600x1200
      }
      
      applyResolution(newRes);
    }
  }
  
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

// Handler for main web page
static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = 
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>ESP32-CAM</title>"
    "<style>"
    "body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;text-align:center;margin:0;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;padding:10px;min-height:100vh}"
    "h1{margin:20px;color:#fff;font-size:28px;text-shadow:2px 2px 4px rgba(0,0,0,0.3);font-weight:300;letter-spacing:2px}"
    "#imageContainer{width:100%;max-width:800px;margin:15px auto;border:3px solid rgba(255,255,255,0.3);border-radius:15px;overflow:hidden;background:#000;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
    "img{width:100%;display:block;min-height:200px}"
    ".controls{margin:15px auto;max-width:800px}"
    "button{background:rgba(255,255,255,0.95);color:#667eea;border:none;padding:8px 18px;text-align:center;"
    "font-size:13px;margin:5px;cursor:pointer;border-radius:20px;min-width:90px;font-weight:600;transition:all 0.3s ease;box-shadow:0 3px 10px rgba(0,0,0,0.2)}"
    "button:hover{transform:translateY(-2px);box-shadow:0 5px 15px rgba(0,0,0,0.3)}"
    "button:active{transform:translateY(0);box-shadow:0 2px 8px rgba(0,0,0,0.2)}"
    ".off{background:linear-gradient(135deg,#f093fb 0%,#f5576c 100%);color:#fff}"
    ".photo{background:linear-gradient(135deg,#4facfe 0%,#00f2fe 100%);color:#fff}"
    ".rotate{background:linear-gradient(135deg,#fa709a 0%,#fee140 100%);color:#fff}"
    ".resolution{background:linear-gradient(135deg,#a8edea 0%,#fed6e3 100%);color:#667eea}"
    ".info{color:rgba(255,255,255,0.8);font-size:12px;margin:10px;font-weight:300}"
    ".slider-container{margin:15px auto;max-width:400px}"
    "input[type=range]{width:100%;height:8px;-webkit-appearance:none;background:rgba(255,255,255,0.2);outline:none;border-radius:10px;backdrop-filter:blur(10px)}"
    "input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:22px;height:22px;background:#fff;cursor:pointer;border-radius:50%;box-shadow:0 2px 8px rgba(0,0,0,0.3)}"
    "input[type=range]::-moz-range-thumb{width:22px;height:22px;background:#fff;cursor:pointer;border-radius:50%;border:none;box-shadow:0 2px 8px rgba(0,0,0,0.3)}"
    ".section{margin:15px auto;padding:15px;max-width:800px;background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);border-radius:15px;border:1px solid rgba(255,255,255,0.2);box-shadow:0 6px 20px rgba(0,0,0,0.1)}"
    ".section h3{color:#fff;margin-bottom:12px;font-weight:400;font-size:16px;letter-spacing:1px}"
    "</style>"
    "</head>"
    "<body>"
    "<h1>ESP32-CAM Control Panel</h1>"
    "<div id='imageContainer'>"
    "<img id='stream' src='/capture' alt='Loading...'>"
    "</div>"
    
    "<div class='section'>"
    "<h3>Flash Control</h3>"
    "<div class='slider-container'>"
    "<label style='display:block;margin:8px 0;font-size:13px;color:rgba(255,255,255,0.9)'>LED Brightness:</label>"
    "<input type='range' min='0' max='255' value='0' id='brightness' oninput='setBrightness(this.value)' step='1'>"
    "<span id='brightnessValue' style='font-size:20px;color:#fff;font-weight:600;text-shadow:0 2px 4px rgba(0,0,0,0.3)'>0</span>"
    "</div>"
    "<button onclick='flashOn()'>FLASH ON</button>"
    "<button class='off' onclick='flashOff()'>FLASH OFF</button>"
    "</div>"
    
    "<div class='section'>"
    "<h3>Photo Capture</h3>"
    "<button class='photo' onclick='takePhoto()'>TAKE PHOTO</button>"
    "</div>"
    
    "<div class='section'>"
    "<h3>Stream Speed</h3>"
    "<div class='slider-container'>"
    "<label style='display:block;margin:8px 0;font-size:13px;color:rgba(255,255,255,0.9)'>Refresh Rate (FPS):</label>"
    "<input type='range' min='100' max='300' value='150' id='fpsSlider' oninput='setFPS(this.value)' step='10'>"
    "<span id='fpsValue' style='font-size:20px;color:#fff;font-weight:600;text-shadow:0 2px 4px rgba(0,0,0,0.3)'>6.7 fps</span>"
    "</div>"
    "</div>"
    
    "<div class='section'>"
    "<h3>Image Orientation</h3>"
    "<button class='rotate' onclick='setRotation(\"normal\")'>NORMAL</button>"
    "<button class='rotate' onclick='setRotation(\"180\")'>ROTATE 180</button>"
    "</div>"
    
    "<div class='section'>"
    "<h3>Resolution</h3>"
    "<button class='resolution' onclick='setResolution(\"vga\")'>VGA 640x480</button>"
    "<button class='resolution' onclick='setResolution(\"svga\")'>SVGA 800x600</button>"
    "<button class='resolution' onclick='setResolution(\"xga\")'>XGA 1024x768</button>"
    "</div>"
    
    "<div class='info' style='background:rgba(0,0,0,0.3);padding:15px;border-radius:10px;margin:20px auto;max-width:600px'>"
    "<div style='font-size:16px;font-weight:600;margin-bottom:8px'>CURRENT SETTINGS</div>"
    "<div style='display:flex;justify-content:space-around;flex-wrap:wrap'>"
    "<div style='margin:8px'><span style='color:rgba(255,255,255,0.7)'>Resolution:</span> <span id='currentRes' style='color:#4facfe;font-weight:600'>VGA 640x480</span></div>"
    "<div style='margin:8px'><span style='color:rgba(255,255,255,0.7)'>Stream Speed:</span> <span id='currentFps' style='color:#4facfe;font-weight:600'>6.7 fps</span></div>"
    "</div>"
    "</div>"
    
    "<script>"
    "let img = document.getElementById('stream');"
    "let refreshInterval;"
    "let currentRefreshRate = 150;"
    "function updateImage() {"
    "  let url = '/capture?t=' + new Date().getTime();"
    "  img.src = url;"
    "}"
    "function flashOn(){"
    "  fetch('/flash/on').then(()=>console.log('Flash ON'));"
    "}"
    "function flashOff(){"
    "  fetch('/flash/off').then(()=>console.log('Flash OFF'));"
    "  document.getElementById('brightness').value = 0;"
    "  document.getElementById('brightnessValue').textContent = '0';"
    "}"
    "function setBrightness(val){"
    "  fetch('/brightness?value=' + val);"
    "  document.getElementById('brightnessValue').textContent = val;"
    "}"
    "function setRotation(mode){"
    "  fetch('/rotate?mode=' + mode).then(()=>console.log('Rotation: ' + mode));"
    "}"
    "function setResolution(res){"
    "  fetch('/resolution?res=' + res).then(()=>{"
    "    console.log('Resolution: ' + res);"
    "    let resText = '';"
    "    if(res === 'vga') resText = 'VGA 640x480';"
    "    else if(res === 'svga') resText = 'SVGA 800x600';"
    "    else if(res === 'xga') resText = 'XGA 1024x768';"
    "    document.getElementById('currentRes').textContent = resText;"
    "    alert('Resolution changed to ' + res.toUpperCase() + '. Wait 2 seconds for camera to adjust.');"
    "  });"
    "}"
    "function setFPS(val){"
    "  currentRefreshRate = parseInt(val);"
    "  let fps = (1000 / currentRefreshRate).toFixed(1);"
    "  document.getElementById('fpsValue').textContent = fps + ' fps';"
    "  document.getElementById('currentFps').textContent = fps + ' fps';"
    "  clearInterval(refreshInterval);"
    "  refreshInterval = setInterval(updateImage, currentRefreshRate);"
    "  console.log('Refresh rate: ' + currentRefreshRate + 'ms (' + fps + ' fps)');"
    "}"
    "function takePhoto(){"
    "  window.open('/photo', '_blank');"
    "}"
    "refreshInterval = setInterval(updateImage, currentRefreshRate);"
    "img.onerror = function(){"
    "  console.log('Image loading error');"
    "};"
    "</script>"
    "</body>"
    "</html>";
  
  httpd_resp_send(req, html, strlen(html));
  return ESP_OK;
}

// Start camera web server
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_open_sockets = 7;
  config.ctrl_port = 32768;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t photo_uri = {
    .uri       = "/photo",
    .method    = HTTP_GET,
    .handler   = photo_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t flash_on_uri = {
    .uri       = "/flash/on",
    .method    = HTTP_GET,
    .handler   = flash_on_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t flash_off_uri = {
    .uri       = "/flash/off",
    .method    = HTTP_GET,
    .handler   = flash_off_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t brightness_uri = {
    .uri       = "/brightness",
    .method    = HTTP_GET,
    .handler   = brightness_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t rotate_uri = {
    .uri       = "/rotate",
    .method    = HTTP_GET,
    .handler   = rotate_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t resolution_uri = {
    .uri       = "/resolution",
    .method    = HTTP_GET,
    .handler   = resolution_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &photo_uri);
    httpd_register_uri_handler(camera_httpd, &flash_on_uri);
    httpd_register_uri_handler(camera_httpd, &flash_off_uri);
    httpd_register_uri_handler(camera_httpd, &brightness_uri);
    httpd_register_uri_handler(camera_httpd, &rotate_uri);
    httpd_register_uri_handler(camera_httpd, &resolution_uri);
    Serial.println("HTTP server started");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting ESP32-CAM...");

  // Initialize I2C for OLED display
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED display not found"));
  } else {
    Serial.println("OLED display initialized");
    updateDisplay("ESP32-CAM", "Initializing...");
  }

  // Configure flash LED with PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_GPIO_NUM, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;  // 640x480
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    updateDisplay("ERROR!", "Camera", "initialization", "failed");
    return;
  }
  Serial.println("Camera initialized successfully");
  updateDisplay("ESP32-CAM", "Camera OK", "Starting WiFi...");

  // Apply default image orientation (normal)
  applyOrientation();

  // Create WiFi Access Point (Hotspot)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  
  Serial.println("========================================");
  Serial.println("ESP32-CAM Hotspot created");
  Serial.print("WiFi SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP Address: ");
  Serial.println(IP);
  Serial.println("========================================");
  
  // Display connection info on OLED
  updateDisplay("WiFi: " + String(ssid), 
                "Pass: " + String(password),
                "IP: " + IP.toString(),
                "Port: 80");
  
  // Start HTTP server
  startCameraServer();
  
  Serial.println("System ready");
}

void loop() {
  delay(1000);
}
