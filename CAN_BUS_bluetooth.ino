#include <SPI.h>
#include <mcp_can.h>
#include <SoftwareSerial.h>

// ---------------------------------------------------------------
// PINES
// ---------------------------------------------------------------
#define CAN_CS_PIN 10
#define BT_RX_PIN 3
#define BT_TX_PIN 4
MCP_CAN CAN(CAN_CS_PIN);
SoftwareSerial BT(BT_RX_PIN, BT_TX_PIN);

// ---------------------------------------------------------------
// TABLA DE COMANDOS CAN
// ---------------------------------------------------------------
struct ComandoCAN {
  const char* nombre;
  unsigned long id;
  byte dlc;
  byte data[8];
};

ComandoCAN tabla[] = {
  {"PARQ_ON", 0x782, 3, {0x1F,0x40,0x48}},
  {"PARQ_OFF", 0x782, 3, {0x00,0x40,0x48}},
  {"FREN_ON", 0x783, 1, {0x08}},
  {"FREN_OFF", 0x783, 1, {0x00}},
  {"BAJAS_ON", 0x784, 1, {0x61}},
  {"BAJAS_OFF", 0x784, 1, {0x00}},
  {"ALTAS_ON", 0x784, 1, {0x63}},
  {"ALTAS_OFF", 0x784, 1, {0x61}},
  {"DIR_DER_ON", 0x782, 3, {0x3A,0x40,0x80}},
  {"DIR_DER_OFF", 0x782, 3, {0x00,0x40,0x80}},
  {"DIR_IZQ_ON", 0x782, 3, {0x25,0x40,0x80}},
  {"DIR_IZQ_OFF", 0x782, 3, {0x00,0x40,0x80}},
  {"PTA_DER_ON", 0x78D, 3, {0x01,0x00,0x00}},
  {"PTA_DER_OFF", 0x78D, 3, {0x00,0x00,0x00}},
  {"PTA_IZQ_ON", 0x78C, 3, {0x01,0x00,0x00}},
  {"PTA_IZQ_OFF", 0x78C, 3, {0x00,0x00,0x00}},
  {"ABIERTO", 0x78A, 1, {0x0A}},
  {"CERRADO", 0x78A, 1, {0x05}}
};
const int totalComandos = sizeof(tabla) / sizeof(tabla[0]);

// ---------------------------------------------------------------
// CONJUNTOS DE TRAMAS PARA EL SEGURO
// ---------------------------------------------------------------
struct TramaCAN {
  unsigned long id;
  byte dlc;
  byte data[8];
};

TramaCAN seguroActivado[] = {
  {0x78A, 1, {0xFF}},
  {0x78C, 1, {0x5E}},
  {0x78D, 3, {0x06, 0x00, 0x00}},
  {0x78E, 3, {0x06, 0x00, 0x00}},
  {0x78F, 1, {0x06}},
  {0x790, 1, {0x01}}
};
const int totalSeguroActivado = sizeof(seguroActivado) / sizeof(seguroActivado[0]);

TramaCAN seguroDesactivado[] = {
  {0x78A, 1, {0x5E}},
  {0x78C, 1, {0x0A}},
  {0x78D, 3, {0x00, 0x00, 0x00}},
  {0x78E, 3, {0x00, 0x00, 0x00}},
  {0x78F, 1, {0x00}},
  {0x790, 1, {0x01}}
};
const int totalSeguroDesactivado = sizeof(seguroDesactivado) / sizeof(seguroDesactivado[0]);

// ---------------------------------------------------------------
// MAPA DE COMANDOS POR NÚMERO
// ---------------------------------------------------------------
struct MapaNumero {
  String codigo;
  const char* comando;
};

MapaNumero mapa[] = {
  {"1", "PARQ_ON"},   {"10", "PARQ_OFF"},
  {"2", "FREN_ON"},   {"20", "FREN_OFF"},
  {"3", "BAJAS_ON"},  {"30", "BAJAS_OFF"},
  {"4", "ALTAS_ON"},  {"40", "ALTAS_OFF"},
  {"5", "DIR_DER_ON"},{"50", "DIR_DER_OFF"},
  {"6", "DIR_IZQ_ON"},{"60", "DIR_IZQ_OFF"},
  {"7", "PTA_DER_ON"},{"70", "PTA_DER_OFF"},
  {"8", "PTA_IZQ_ON"},{"80", "PTA_IZQ_OFF"},
  {"9", "ABIERTO"},   {"90", "CERRADO"}
};
const int totalMapa = sizeof(mapa) / sizeof(mapa[0]);

// ---------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  BT.begin(9600);
  Serial.println("Iniciando MCP2515...");
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_125KBPS, MCP_8MHZ)) {
    Serial.println("Error al iniciar CAN, reintentando...");
    delay(500);
  }
  CAN.setMode(MCP_NORMAL);
  Serial.println("CAN OK a 125 Kbps");
  Serial.println("Esperando numeros por Bluetooth...");
}

// ---------------------------------------------------------------
// LOOP PRINCIPAL
// ---------------------------------------------------------------
void loop() {
  if (BT.available()) {
    String recibido = BT.readStringUntil('\n');
    recibido.trim();
    if (recibido.length() > 0) {
      ejecutarNumero(recibido);
    }
  }
}

// ---------------------------------------------------------------
// EJECUTA COMANDO RECIBIDO
// ---------------------------------------------------------------
void ejecutarNumero(String cod) {
  cod.trim();

  if (cod == "99") {
    enviarConjuntoCAN(seguroActivado, totalSeguroActivado, "SEGURO ACTIVADO");
    secuenciaSeguroActivado();
    return;
  }
  if (cod == "0") {
    enviarConjuntoCAN(seguroDesactivado, totalSeguroDesactivado, "SEGURO DESACTIVADO");
    secuenciaSeguroDesactivado();
    return;
  }

  for (int i = 0; i < totalMapa; i++) {
    if (mapa[i].codigo == cod) {
      enviarComandoPorNombre(mapa[i].comando);
      return;
    }
  }

  Serial.print("Codigo no reconocido: ");
  Serial.println(cod);
}

// ---------------------------------------------------------------
// ENVÍA UN CONJUNTO DE TRAMAS
// ---------------------------------------------------------------
void enviarConjuntoCAN(TramaCAN* tramas, int total, const char* etiqueta) {
  for (int i = 0; i < total; i++) {
    byte resultado = CAN.sendMsgBuf(tramas[i].id, 0, tramas[i].dlc, tramas[i].data);
    delay(10); // Estabilidad en el bus
    Serial.print(etiqueta);
    Serial.print(" -> ID 0x");
    Serial.print(tramas[i].id, HEX);
    if (resultado == CAN_OK) Serial.println(" enviado OK");
    else Serial.println(" ERROR al enviar");
  }
}

// ---------------------------------------------------------------
// SECUENCIA VISUAL AL ACTIVAR SEGURO
// ---------------------------------------------------------------
void secuenciaSeguroActivado() {
  enviarComandoPorNombre("DIR_DER_ON");
  enviarComandoPorNombre("DIR_IZQ_ON");
  delay(3000);
  enviarComandoPorNombre("DIR_DER_OFF");
  enviarComandoPorNombre("DIR_IZQ_OFF");
}

// ---------------------------------------------------------------
// SECUENCIA VISUAL AL DESACTIVAR SEGURO
// ---------------------------------------------------------------
void secuenciaSeguroDesactivado() {
  for (int i = 0; i < 3; i++) {
    enviarComandoPorNombre("DIR_DER_ON");
    enviarComandoPorNombre("DIR_IZQ_ON");
    delay(1000);
    enviarComandoPorNombre("DIR_DER_OFF");
    enviarComandoPorNombre("DIR_IZQ_OFF");
    delay(300);
  }
}

// ---------------------------------------------------------------
// BUSCA Y ENVÍA COMANDO POR NOMBRE
// ---------------------------------------------------------------
void enviarComandoPorNombre(const char* nombre) {
  for (int i = 0; i < totalComandos; i++) {
    if (strcmp(tabla[i].nombre, nombre) == 0) {
      byte resultado = CAN.sendMsgBuf(tabla[i].id, 0, tabla[i].dlc, tabla[i].data);
      delay(10); // Estabilidad en el bus
      if (resultado == CAN_OK) {
        Serial.print("Enviado -> ");
        Serial.println(nombre);
      } else {
        Serial.print("ERROR enviando -> ");
        Serial.println(nombre);
      }
      return;
    }
  }
}

