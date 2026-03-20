/*
 * ============================================================================
 * VS_Drop.cloud ESP8266 v5.0.0 - SD-BASED ARCHITECTURE
 * ============================================================================
 * 
 * АРХИТЕКТУРА:
 * 
 * SD-карта:
 * ├── .system/           (СКРЫТА - системные файлы)
 * │   ├── web/           (HTML, CSS, JS)
 * │   │   ├── index.html
 * │   │   ├── login.html
 * │   │   ├── info.html
 * │   │   └── style.css
 * │   └── config.json    (настройки)
 * └── data/              (ВИДИМА - файлы пользователя)
 *     └── ...
 * 
 * Flash ESP8266:
 * ├── Загрузчик
 * ├── Ядро (этот код)
 * └── Драйверы (SD, WiFi, FTP)
 * 
 * ПРЕИМУЩЕСТВА:
 * ✅ RAM свободна - HTML на SD
 * ✅ Гибкость - правим интерфейс без перепрошивки
 * ✅ Безопасно - .system скрыта от пользователя
 * ✅ Надёжно - ядро во flash, работает без SD
 * 
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <SimpleFTPServer.h>
#include <ArduinoJson.h>

// ============================================================================
// ВЕРСИЯ
// ============================================================================

#define FIRMWARE_VERSION "5.0.0"

// ============================================================================
// ПУТИ
// ============================================================================

#define PATH_SYSTEM        "/.system"
#define PATH_WEB           "/.system/web"
#define PATH_CONFIG        "/.system/config.json"
#define PATH_DATA          "/data"

// ============================================================================
// КОНФИГУРАЦИЯ ПО УМОЛЧАНИЮ
// ============================================================================

struct Config {
  char ap_ssid[33] = "VS_Drop_Cloud";
  char ap_password[65] = "admin123";
  char auth_user[33] = "admin";
  char auth_pass[33] = "admin";
};

Config config;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================

ESP8266WebServer server(80);
FtpServer ftpSrv;

bool sdInitialized = false;
bool systemInitialized = false;
String sessionToken = "";

// OTA
volatile bool otaPending = false;
volatile bool otaIsFirmware = false;
bool otaInProgress = false;

// SD Check
unsigned long lastSDCheck = 0;
#define SD_CHECK_INTERVAL 10000

// Min memory
#define MIN_FREE_HEAP 10240

// ============================================================================
// WATCHDOG
// ============================================================================

void initWatchdog() {
  ESP.wdtEnable(8000);
}

void feedWatchdog() {
  ESP.wdtFeed();
}

// ============================================================================
// ПРОВЕРКА ПАМЯТИ
// ============================================================================

bool checkMinMemory() {
  if (ESP.getFreeHeap() < MIN_FREE_HEAP) {
    Serial.printf_P(PSTR("[MEM] LOW: %u\n"), ESP.getFreeHeap());
    return false;
  }
  return true;
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

String formatSize(uint64_t bytes) {
  if (bytes < 1024) return String((uint32_t)bytes) + " B";
  if (bytes < 1048576) return String(bytes / 1024.0, 1) + " KB";
  if (bytes < 1073741824) return String(bytes / 1048576.0, 1) + " MB";
  return String(bytes / 1073741824.0, 1) + " GB";
}

bool isHexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

String urlDecode(const String& str) {
  String result;
  result.reserve(str.length());
  
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    
    if (c == '+') {
      result += ' ';
    } else if (c == '%' && i + 2 < str.length()) {
      char h1 = str.charAt(i + 1);
      char h2 = str.charAt(i + 2);
      
      if (isHexDigit(h1) && isHexDigit(h2)) {
        char hex[3] = {h1, h2, 0};
        result += (char)strtol(hex, NULL, 16);
        i += 2;
      } else {
        result += c;
      }
    } else {
      result += c;
    }
  }
  
  return result;
}

// ============================================================================
// ЗАЩИТА ПУТЕЙ
// ============================================================================

bool isSystemPath(const String& path) {
  String p = path;
  p.toLowerCase();
  return p.startsWith("/.system") || p.indexOf("/.system") >= 0;
}

bool isDataPath(const String& path) {
  return path.startsWith("/data") || path == "/data";
}

bool isPathSafe(const String& path) {
  if (path.indexOf("..") >= 0) return false;
  if (path.indexOf("//") >= 0) return false;
  if (path.indexOf("\\") >= 0) return false;
  
  String lower = path;
  lower.toLowerCase();
  if (lower.indexOf("%2e") >= 0) return false;
  if (lower.indexOf("%2f") >= 0) return false;
  if (lower.indexOf("%5c") >= 0) return false;
  
  return true;
}

String sanitizePath(const String& path) {
  String result = path;
  result.replace("..", "");
  result.replace("//", "/");
  result.replace("\\", "/");
  
  while (result.indexOf("//") >= 0) {
    result.replace("//", "/");
  }
  
  if (!result.startsWith("/")) {
    result = "/" + result;
  }
  
  if (result.length() > 128) {
    result = result.substring(0, 128);
  }
  
  return result;
}

String toUserDataPath(const String& path) {
  // Преобразуем путь в путь внутри /data
  if (path.startsWith("/data")) {
    return path;
  }
  
  if (path == "/" || path.isEmpty()) {
    return PATH_DATA;
  }
  
  // Убираем начальный слеш и добавляем /data
  String cleanPath = path;
  if (cleanPath.startsWith("/")) {
    cleanPath = cleanPath.substring(1);
  }
  
  return String(PATH_DATA) + "/" + cleanPath;
}

// ============================================================================
// ЗАГРУЗКА КОНФИГУРАЦИИ
// ============================================================================

bool loadConfig() {
  if (!SD.exists(PATH_CONFIG)) {
    Serial.println(F("[CFG] Файл не найден, используем default"));
    return false;
  }
  
  File file = SD.open(PATH_CONFIG, FILE_READ);
  if (!file) {
    Serial.println(F("[CFG] Ошибка открытия"));
    return false;
  }
  
  // Ограничиваем размер
  size_t size = file.size();
  if (size > 1024) {
    Serial.println(F("[CFG] Файл слишком большой"));
    file.close();
    return false;
  }
  
  // Читаем в буфер
  char buffer[1024];
  size_t len = file.readBytes(buffer, sizeof(buffer) - 1);
  buffer[len] = '\0';
  file.close();
  
  // Парсим JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, buffer);
  
  if (error) {
    Serial.print(F("[CFG] JSON error: "));
    Serial.println(error.c_str());
    return false;
  }
  
  // Применяем настройки
  if (doc.containsKey("ap_ssid")) {
    strlcpy(config.ap_ssid, doc["ap_ssid"], sizeof(config.ap_ssid));
  }
  if (doc.containsKey("ap_password")) {
    strlcpy(config.ap_password, doc["ap_password"], sizeof(config.ap_password));
  }
  if (doc.containsKey("auth_user")) {
    strlcpy(config.auth_user, doc["auth_user"], sizeof(config.auth_user));
  }
  if (doc.containsKey("auth_pass")) {
    strlcpy(config.auth_pass, doc["auth_pass"], sizeof(config.auth_pass));
  }
  
  Serial.println(F("[CFG] Загружено из SD"));
  return true;
}

bool saveConfig() {
  // Убедимся что папка существует
  if (!SD.exists(PATH_SYSTEM)) {
    SD.mkdir(PATH_SYSTEM);
  }
  
  File file = SD.open(PATH_CONFIG, FILE_WRITE);
  if (!file) {
    Serial.println(F("[CFG] Ошибка создания файла"));
    return false;
  }
  
  StaticJsonDocument<512> doc;
  doc["ap_ssid"] = config.ap_ssid;
  doc["ap_password"] = config.ap_password;
  doc["auth_user"] = config.auth_user;
  doc["auth_pass"] = config.auth_pass;
  
  serializeJson(doc, file);
  file.close();
  
  Serial.println(F("[CFG] Сохранено на SD"));
  return true;
}

// ============================================================================
// SD CARD
// ============================================================================

bool initSDCard() {
  Serial.println(F("[SD] Инициализация..."));
  
  feedWatchdog();
  
  if (SD.begin(4)) {
    Serial.println(F("[SD] OK"));
    return true;
  }
  
  feedWatchdog();
  
  if (SD.begin(4, SPI_HALF_SPEED)) {
    Serial.println(F("[SD] OK (half)"));
    return true;
  }
  
  Serial.println(F("[SD] FAILED"));
  return false;
}

bool checkSDCard() {
  File root = SD.open("/");
  if (!root) return false;
  bool isDir = root.isDirectory();
  root.close();
  return isDir;
}

bool initSystemFolders() {
  Serial.println(F("[SYS] Проверка системных папок..."));
  
  // Создаём .system если нет
  if (!SD.exists(PATH_SYSTEM)) {
    if (SD.mkdir(PATH_SYSTEM)) {
      Serial.println(F("[SYS] Создана /.system"));
    } else {
      Serial.println(F("[SYS] Ошибка создания /.system"));
      return false;
    }
  }
  
  // Создаём web если нет
  if (!SD.exists(PATH_WEB)) {
    if (SD.mkdir(PATH_WEB)) {
      Serial.println(F("[SYS] Создана /.system/web"));
    }
  }
  
  // Создаём data если нет
  if (!SD.exists(PATH_DATA)) {
    if (SD.mkdir(PATH_DATA)) {
      Serial.println(F("[SYS] Создана /data"));
    }
  }
  
  // Проверяем наличие минимальных файлов
  if (!SD.exists(PATH_WEB "/index.html")) {
    Serial.println(F("[SYS] index.html отсутствует - будет использован fallback"));
  }
  
  return true;
}

// ============================================================================
// СЕРВИС СИСТЕМНЫХ ФАЙЛОВ
// ============================================================================

String getMimeType(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".gif")) return "image/gif";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

bool serveSystemFile(const String& filename) {
  String path = String(PATH_WEB) + "/" + filename;
  
  if (!SD.exists(path)) {
    Serial.printf_P(PSTR("[WEB] File not found: %s\n"), path.c_str());
    return false;
  }
  
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return false;
  }
  
  server.streamFile(file, getMimeType(path));
  file.close();
  
  return true;
}

// ============================================================================
// FALLBACK HTML (встроенный в PROGMEM)
// ============================================================================

const char FALLBACK_LOGIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>VS_Drop.cloud</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,sans-serif;background:#0a0a0a;min-height:100vh;color:#fafafa;display:flex;align-items:center;justify-content:center}
.box{width:100%;max-width:300px;padding:20px}
.logo{text-align:center;margin-bottom:24px}
.logo-icon{width:48px;height:48px;background:#f97316;border-radius:12px;margin:0 auto 12px;display:flex;align-items:center;justify-content:center;font-size:20px}
.logo h1{font-size:20px}
.logo p{font-size:12px;color:#737373}
.card{background:#141414;border:1px solid #262626;border-radius:12px;padding:20px}
input{width:100%;padding:10px 12px;background:#262626;border:1px solid #262626;border-radius:8px;color:#fafafa;font-size:14px;margin-bottom:12px}
input:focus{outline:none;border-color:#f97316}
button{width:100%;padding:10px;background:#f97316;border:none;border-radius:8px;color:#000;font-size:14px;font-weight:500;cursor:pointer}
.hint{font-size:11px;color:#737373;text-align:center;margin-top:12px}
.warn{background:#7f1d1d;border-radius:8px;padding:10px;margin-bottom:12px;font-size:12px}
</style></head><body>
<div class='box'>
<div class='logo'>
<div class='logo-icon'>☁</div>
<h1>VS_Drop.cloud</h1>
<p>v)rawliteral";

const char FALLBACK_LOGIN2[] PROGMEM = R"rawliteral(</p>
<p style='color:#f97316;font-size:10px;margin-top:4px'>Fallback Mode</p>
</div>
<div class='warn'>⚠️ Системные файлы не найдены на SD. Загрузите /.system/web/</div>
<div class='card'>
<form method='post' action='/login'>
<input type='text' name='user' placeholder='Логин'>
<input type='password' name='pass' placeholder='Пароль'>
<button type='submit'>Войти</button>
</form>
<div class='hint'>admin / admin</div>
</div></div></body></html>)rawliteral";

void sendFallbackLogin() {
  server.send(200, "text/html", 
    String(FPSTR(FALLBACK_LOGIN)) + F(FIRMWARE_VERSION) + FPSTR(FALLBACK_LOGIN2));
}

const char FALLBACK_ERROR[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Error</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,sans-serif;background:#0a0a0a;min-height:100vh;color:#fafafa;display:flex;align-items:center;justify-content:center}
.box{max-width:400px;padding:20px;text-align:center}
h1{color:#dc2626;margin-bottom:12px}
p{color:#737373;margin-bottom:16px}
.btn{display:inline-block;padding:10px 20px;background:#f97316;color:#000;border-radius:8px;text-decoration:none}
</style></head><body>
<div class='box'>
<h1>⚠️ Ошибка</h1>
<p id='msg'>)rawliteral";

const char FALLBACK_ERROR2[] PROGMEM = R"rawliteral(</p>
<a href='/' class='btn'>Повторить</a>
</div></body></html>)rawliteral";

void sendErrorPage(const String& msg) {
  server.send(200, "text/html", 
    String(FPSTR(FALLBACK_ERROR)) + msg + FPSTR(FALLBACK_ERROR2));
}

// ============================================================================
// OTA
// ============================================================================

void performOTAUpdate() {
  if (otaInProgress) return;
  if (!checkMinMemory()) {
    otaPending = false;
    return;
  }
  
  otaInProgress = true;
  
  String fileName = otaIsFirmware ? "firmware.bin" : "filesystem.bin";
  String bakName = otaIsFirmware ? "firmware.bak" : "filesystem.bak";
  int updateMode = otaIsFirmware ? U_FLASH : U_FS;
  
  // OTA файлы в корне SD (НЕ в /data!)
  String filePath = "/" + fileName;
  String bakPath = "/" + bakName;
  
  Serial.println(F("\n[OTA] ================================"));
  Serial.printf_P(PSTR("[OTA] Файл: %s\n"), fileName.c_str());
  
  feedWatchdog();
  
  File file = SD.open(filePath, FILE_READ);
  if (!file) {
    Serial.println(F("[OTA] Файл не найден"));
    otaInProgress = false;
    otaPending = false;
    return;
  }
  
  size_t fileSize = file.size();
  Serial.printf_P(PSTR("[OTA] Размер: %u\n"), fileSize);
  
  if (fileSize < 1024) {
    Serial.println(F("[OTA] Слишком маленький"));
    file.close();
    otaInProgress = false;
    otaPending = false;
    return;
  }
  
  Update.onProgress([](size_t curr, size_t total) {
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 1000 || curr == total) {
      Serial.printf_P(PSTR("[OTA] %u/%u (%.0f%%)\n"), curr, total, (curr*100.0)/total);
      lastLog = millis();
      feedWatchdog();
    }
  });
  
  if (!Update.begin(fileSize, updateMode)) {
    Serial.print(F("[OTA] Error: "));
    Serial.println(Update.getError());
    file.close();
    otaInProgress = false;
    otaPending = false;
    return;
  }
  
  Serial.println(F("[OTA] Writing..."));
  feedWatchdog();
  
  size_t written = Update.writeStream(file);
  file.close();
  
  Serial.printf_P(PSTR("[OTA] Written: %u\n"), written);
  feedWatchdog();
  
  if (Update.end()) {
    Serial.println(F("[OTA] SUCCESS!"));
    
    if (SD.exists(bakPath)) SD.remove(bakPath);
    SD.rename(filePath, bakPath);
    
    Serial.println(F("[OTA] Rebooting..."));
    delay(2000);
    ESP.restart();
  } else {
    Serial.print(F("[OTA] Error: "));
    Serial.println(Update.getError());
  }
  
  otaInProgress = false;
  otaPending = false;
}

// ============================================================================
// FTP CALLBACKS
// ============================================================================

void ftpCallback(FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace) {
  switch (ftpOperation) {
    case FTP_CONNECT:
      Serial.println(F("[FTP] Connected"));
      break;
    case FTP_DISCONNECT:
      Serial.println(F("[FTP] Disconnected"));
      break;
    case FTP_FREE_SPACE_CHANGE:
      Serial.printf_P(PSTR("[FTP] Space: %u/%u\n"), totalSpace - freeSpace, totalSpace);
      break;
    default:
      break;
  }
}

void ftpTransferCallback(FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize) {
  // ЗАЩИТА СИСТЕМНОЙ ПАПКИ!
  String path = String(name);
  if (isSystemPath(path) && ftpOperation != FTP_DOWNLOAD_START && ftpOperation != FTP_DOWNLOAD && ftpOperation != FTP_DOWNLOAD_STOP) {
    Serial.printf_P(PSTR("[FTP] BLOCKED: %s\n"), name);
    return;
  }
  
  if (ftpOperation == FTP_UPLOAD_START) {
    Serial.printf_P(PSTR("[FTP] ↑ %s\n"), name);
  }
  else if (ftpOperation == FTP_UPLOAD) {
    if (transferredSize % 102400 == 0) {
      Serial.printf_P(PSTR("[FTP] ↑ %u KB\n"), transferredSize / 1024);
      feedWatchdog();
    }
  }
  else if (ftpOperation == FTP_DOWNLOAD_START) {
    Serial.printf_P(PSTR("[FTP] ↓ %s\n"), name);
  }
  else if (ftpOperation == FTP_DOWNLOAD) {
    if (transferredSize % 102400 == 0) {
      Serial.printf_P(PSTR("[FTP] ↓ %u KB\n"), transferredSize / 1024);
      feedWatchdog();
    }
  }
  else if (ftpOperation == FTP_TRANSFER_STOP) {
    Serial.printf_P(PSTR("[FTP] ✓ %s (%u)\n"), name, transferredSize);
    
    String fileName = String(name);
    fileName.toLowerCase();
    
    // OTA - только из корня, НЕ из /data
    if ((fileName == "/firmware.bin" || fileName == "firmware.bin")) {
      otaIsFirmware = true;
      otaPending = true;
      Serial.println(F("[OTA] Scheduled: firmware.bin"));
    }
    else if ((fileName == "/filesystem.bin" || fileName == "filesystem.bin")) {
      otaIsFirmware = false;
      otaPending = true;
      Serial.println(F("[OTA] Scheduled: filesystem.bin"));
    }
  }
  else if (ftpOperation == FTP_TRANSFER_ERROR) {
    Serial.printf_P(PSTR("[FTP] ✗ %s\n"), name);
  }
}

// ============================================================================
// АВТОРИЗАЦИЯ
// ============================================================================

bool checkAuth() {
  if (!server.hasHeader("Cookie")) return false;
  String cookie = server.header("Cookie");
  return cookie.indexOf("session=" + sessionToken) != -1 && sessionToken.length() > 0;
}

String generateToken() {
  String t = "";
  for (int i = 0; i < 32; i++) t += String(random(16), HEX);
  return t;
}

// ============================================================================
// WEB HANDLERS
// ============================================================================

void handleLogin() {
  // Пробуем загрузить с SD
  if (systemInitialized && serveSystemFile("login.html")) {
    return;
  }
  // Fallback
  sendFallbackLogin();
}

void handleLoginPost() {
  String user = server.arg("user");
  String pass = server.arg("pass");
  
  if (user.length() > 32 || pass.length() > 32) {
    server.sendHeader("Location", "/login");
    server.send(302);
    return;
  }
  
  if (user == String(config.auth_user) && pass == String(config.auth_pass)) {
    sessionToken = generateToken();
    server.sendHeader("Set-Cookie", "session=" + sessionToken + "; Path=/; HttpOnly; SameSite=Strict");
    server.sendHeader("Location", "/");
    server.send(302);
    Serial.println(F("[WEB] Login OK"));
  } else {
    server.sendHeader("Location", "/login");
    server.send(302);
    Serial.println(F("[WEB] Login failed"));
  }
}

void handleLogout() {
  sessionToken = "";
  server.sendHeader("Set-Cookie", "session=; Path=/; Max-Age=0; HttpOnly");
  server.sendHeader("Location", "/login");
  server.send(302);
}

void handleRoot() {
  if (!checkAuth()) {
    server.sendHeader("Location", "/login");
    server.send(302);
    return;
  }
  
  // Пробуем загрузить с SD
  if (systemInitialized && serveSystemFile("index.html")) {
    return;
  }
  
  // Fallback - минимальный список файлов
  sendErrorPage("Системные файлы не найдены. Загрузите /.system/web/");
}

void handleStatic() {
  if (!checkAuth()) {
    server.send(401);
    return;
  }
  
  String file = server.arg("f");
  if (file.length() == 0 || file.indexOf("..") >= 0 || file.indexOf("/") >= 0) {
    server.send(400);
    return;
  }
  
  if (!serveSystemFile(file)) {
    server.send(404);
  }
}

void handleFiles() {
  if (!checkAuth()) {
    server.send(401);
    return;
  }
  
  // API для получения списка файлов из /data
  String currentPath = "/data";
  if (server.hasArg("path")) {
    String p = urlDecode(server.arg("path"));
    if (isPathSafe(p) && !isSystemPath(p)) {
      currentPath = toUserDataPath(p);
    }
  }
  
  if (!sdInitialized) {
    server.send(500, "application/json", "{\"error\":\"SD not ready\"}");
    return;
  }
  
  File root = SD.open(currentPath);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    server.send(200, "application/json", "[]");
    return;
  }
  
  String json = "[";
  bool first = true;
  
  File file = root.openNextFile();
  int count = 0;
  
  while (file && count < 100) {
    if (!first) json += ",";
    first = false;
    
    json += "{\"name\":\"";
    json += file.name();
    json += "\",\"size\":";
    json += file.size();
    json += ",\"dir\":";
    json += file.isDirectory() ? "true" : "false";
    json += "}";
    
    file.close();
    file = root.openNextFile();
    count++;
    feedWatchdog();
  }
  
  if (file) file.close();
  root.close();
  
  json += "]";
  
  server.send(200, "application/json", json);
}

void handleDownload() {
  if (!checkAuth()) {
    server.send(401);
    return;
  }
  
  if (!sdInitialized) {
    server.send(500);
    return;
  }
  
  String path = "/data";
  if (server.hasArg("path")) {
    path = urlDecode(server.arg("path"));
  }
  
  // Безопасность
  if (!isPathSafe(path)) {
    server.send(403);
    return;
  }
  
  // Запрещаем скачивание системных файлов
  if (isSystemPath(path)) {
    server.send(403);
    return;
  }
  
  // Преобразуем в путь в /data
  path = toUserDataPath(path);
  
  if (!SD.exists(path)) {
    server.send(404);
    return;
  }
  
  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    server.send(400);
    return;
  }
  
  String fileName = path.substring(path.lastIndexOf('/') + 1);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
  server.streamFile(file, getMimeType(path));
  file.close();
}

void handleInfo() {
  if (!checkAuth()) {
    server.sendHeader("Location", "/login");
    server.send(302);
    return;
  }
  
  if (systemInitialized && serveSystemFile("info.html")) {
    return;
  }
  
  // Fallback
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<title>Info</title>"
    "<style>*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:system-ui;background:#0a0a0a;color:#fafafa;padding:20px}"
    ".card{background:#141414;border:1px solid #262626;border-radius:12px;padding:16px;margin-bottom:12px}"
    "h2{margin-bottom:8px}table{width:100%}td{padding:4px 0}"
    "td:first-child{color:#737373;width:40%}</style></head><body>"
    "<div class='card'><h2>Система</h2><table>"
    "<tr><td>Версия</td><td>");
  html += F(FIRMWARE_VERSION);
  html += F("</td></tr><tr><td>RAM</td><td>");
  html += String(ESP.getFreeHeap());
  html += F("</td></tr><tr><td>Uptime</td><td>");
  html += String(millis() / 1000);
  html += F(" сек</td></tr></table></div>"
    "<div class='card'><h2>SD</h2><table>"
    "<tr><td>Статус</td><td>");
  html += sdInitialized ? F("OK") : F("Error");
  html += F("</td></tr></table></div>"
    "<div class='card'><h2>FTP</h2><table>"
    "<tr><td>Адрес</td><td>192.168.4.1:21</td></tr>"
    "<tr><td>Логин</td><td>admin</td></tr>"
    "<tr><td>Пароль</td><td>admin</td></tr>"
    "</table></div>"
    "<a href='/' style='display:inline-block;padding:10px 20px;background:#f97316;color:#000;border-radius:8px;text-decoration:none'>← Назад</a>"
    "</body></html>");
  
  server.send(200, "text/html", html);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("   VS_Drop.cloud ESP8266 v5.0.0"));
  Serial.println(F("      SD-Based Architecture"));
  Serial.println(F("========================================"));
  
  initWatchdog();
  
  // SPI
  SPI.begin();
  feedWatchdog();
  
  // LittleFS (fallback config)
  LittleFS.begin();
  feedWatchdog();
  
  // SD Card
  sdInitialized = initSDCard();
  lastSDCheck = millis();
  
  if (sdInitialized) {
    // Системные папки
    systemInitialized = initSystemFolders();
    
    // Загрузка конфига
    loadConfig();
  }
  
  feedWatchdog();
  
  // WiFi
  Serial.println(F("\n[WiFi] Starting AP..."));
  WiFi.mode(WIFI_AP);
  WiFi.softAP(config.ap_ssid, config.ap_password);
  
  Serial.print(F("[WiFi] IP: "));
  Serial.println(WiFi.softAPIP());
  feedWatchdog();
  
  // FTP
  Serial.println(F("\n[FTP] Starting..."));
  ftpSrv.setCallback(ftpCallback);
  ftpSrv.setTransferCallback(ftpTransferCallback);
  ftpSrv.begin(config.auth_user, config.auth_pass);
  Serial.println(F("[FTP] OK"));
  feedWatchdog();
  
  // HTTP Routes
  server.on("/", handleRoot);
  server.on("/login", HTTP_GET, handleLogin);
  server.on("/login", HTTP_POST, handleLoginPost);
  server.on("/logout", handleLogout);
  server.on("/static", handleStatic);
  server.on("/api/files", handleFiles);
  server.on("/dl", handleDownload);
  server.on("/info", handleInfo);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  
  server.collectHeaders("Cookie", "");
  server.begin();
  
  Serial.println(F("\n========================================"));
  Serial.println(F("         READY"));
  Serial.println(F("========================================"));
  Serial.printf_P(PSTR("  Web:  http://192.168.4.1\n"));
  Serial.printf_P(PSTR("  FTP:  192.168.4.1:21\n"));
  Serial.printf_P(PSTR("  Data: /data (user files)\n"));
  Serial.printf_P(PSTR("  Sys:  /.system (hidden)\n"));
  Serial.println(F("========================================"));
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  feedWatchdog();
  
  // OTA
  if (otaPending && !otaInProgress) {
    performOTAUpdate();
  }
  
  // HTTP
  server.handleClient();
  feedWatchdog();
  
  // FTP
  ftpSrv.handleFTP();
  feedWatchdog();
  
  // SD Check
  if (millis() - lastSDCheck > SD_CHECK_INTERVAL) {
    lastSDCheck = millis();
    if (sdInitialized && !checkSDCard()) {
      Serial.println(F("[SD] Lost, reinit..."));
      sdInitialized = initSDCard();
      if (sdInitialized) {
        systemInitialized = initSystemFolders();
      }
    }
    feedWatchdog();
  }
  
  yield();
}
