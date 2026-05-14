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