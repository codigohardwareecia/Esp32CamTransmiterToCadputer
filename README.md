O código está funcional mas os detalhes da implementação vou colocar aqui ainda e vou dar uma limpaa no código depois

Este código deve ser gravado no ESP32CAM MB
```
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h" // Importante estar aqui em cima!

// Configurações dos pinos 
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

const char* ssid = "ESP32-CAM-STREAM";
const char* password = "cardputer123";

// --- FUNÇÃO DE CAPTURA (HANDLER) ---
esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Falha na captura da câmera");
        return httpd_resp_send_500(req);
    }
    
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

// --- FUNÇÃO QUE INICIA O SERVIDOR ---
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t capture_uri = {
            .uri      = "/capture",
            .method   = HTTP_GET,
            .handler  = capture_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &capture_uri);
        Serial.println("Servidor HTTP iniciado");
    }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA; // Resolução 640x480
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Inicia a Câmera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Erro ao iniciar câmera: 0x%x", err);
    return;
  }

  // --- CÓDIGO NOVO PARA INVERTER A IMAGEM ---
  sensor_t * s = esp_camera_sensor_get();
  // Inverte verticalmente (0 = normal, 1 = invertido)
  // Espelha horizontalmente (0 = normal, 1 = espelhado)
  s->set_vflip(s, 1);   // Inverte vertical
  //s->set_hmirror(s, 1); // Inverte horizontal

  // Configura Wi-Fi como Access Point
  WiFi.softAP(ssid, password);
  Serial.println("");
  Serial.print("Wi-Fi SSID: "); Serial.println(ssid);
  Serial.print("IP para o Cardputer: "); Serial.println(WiFi.softAPIP());

  startCameraServer();
}

void loop() {
  delay(1); // O servidor roda em background
}
```

Este código deve ser gravado no Cardputer ADV, tem que instalar TJpg_Decoder na Lib Manager
```
#include "M5Cardputer.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Forçamos a inclusão da biblioteca de JPEG
#include <TJpg_Decoder.h>

const char* ssid = "ESP32-CAM-STREAM";
const char* password = "cardputer123";
const char* serverUrl = "http://192.168.4.1/capture";

// Função de callback para o decodificador desenhar no LCD do M5
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= M5.Display.height()) return false;
    M5.Display.pushImage(x, y, w, h, bitmap);
    return true;
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
   M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE);
    M5.Lcd.setSwapBytes(true);
    
    // CONFIGURAÇÃO DO DECODER
    // Se o erro persistir, tente usar TJpgDec (sem o _ e sem er) 
    // mas o padrão da biblioteca do Bodmer é este:
    
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);

    M5.Display.printf("Conectando: %s", ssid);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Display.print(".");
    }
    M5.Display.fillScreen(BLACK);
}

void loop() {
    M5.update();

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.setTimeout(1500); 

        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            int len = http.getSize();
            if (len > 0) {
                uint8_t* buff = (uint8_t*)malloc(len);
                if (buff) {
                    WiFiClient* stream = http.getStreamPtr();
                    stream->readBytes(buff, len);
                    
                    // DESENHA A IMAGEM
                    TJpgDec.drawJpg(0, 0, buff, len);
                    
                    free(buff); 
                }
            }
        }
        http.end();
    } else {
        M5.Display.setCursor(0,0);
        M5.Display.println("Reconectando WiFi...");
        WiFi.begin(ssid, password);
        delay(2000);
    }
}
```

