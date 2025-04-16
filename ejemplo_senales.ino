const int pinNA = 9;       // Simula NO DISPENSANDO (~490 Hz)
const int pinNC = 11;      // Simula DISPENSANDO (~980 Hz)
const int pinLector = 2;   // Entrada de lectura PWM

enum EstadoDispensador {
  NO_DISPENSANDO,
  DISPENSANDO,
  VANDALISMO
};

EstadoDispensador estadoActual = VANDALISMO;

void setup() {
  pinMode(pinNA, OUTPUT);
  pinMode(pinNC, OUTPUT);
  pinMode(pinLector, INPUT);
  Serial.begin(9600);

  // Ambos pines siempre activos con señales PWM (duty 50%)
  analogWrite(pinNA, 127);     // 50% duty en pin 9 (~490 Hz)
  analogWrite(pinNC, 127);     // 50% duty en pin 11 (~980 Hz)

  configurarFrecuenciaAlta();  // Asegurarse de que pin 11 esté configurado a la frecuencia correcta
}

void loop() {
  EstadoDispensador nuevoEstado = detectarEstado();

  if (nuevoEstado != estadoActual) {
    estadoActual = nuevoEstado;

    switch (estadoActual) {
      case NO_DISPENSANDO:
        Serial.println("⏸ Estado: NO DISPENSANDO (pin 9)");
        break;
      case DISPENSANDO:
        Serial.println("✅ Estado: DISPENSANDO (pin 11)");
        break;
      case VANDALISMO:
        Serial.println("⚠️ Estado: VANDALISMO o desconectado");
        break;
    }
  }

  delay(1000);
}

EstadoDispensador detectarEstado() {
  unsigned long highTime = pulseIn(pinLector, HIGH, 30000);
  unsigned long lowTime = pulseIn(pinLector, LOW, 30000);

  if (highTime == 0 || lowTime == 0) return VANDALISMO;

  float freq = 1000000.0 / (highTime + lowTime);  // Calcular frecuencia (Hz)
  Serial.print("📡 Frecuencia detectada: ");
  Serial.println(freq, 1);

  if (freq > 450 && freq < 530) {
    return NO_DISPENSANDO;  // pin 9 (~490 Hz)
  } else if (freq > 900 && freq < 1050) {
    return DISPENSANDO;     // pin 11 (~980 Hz)
  } else {
    return VANDALISMO;      // Señal desconocida o desconexión
  }
}

void configurarFrecuenciaAlta() {
  // Configurar Timer2 para Fast PWM en pin 11 (~980 Hz)
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);  // PWM en OC2A (pin 11)
  TCCR2B = _BV(CS22);  // Prescaler 64 → ~980 Hz
  OCR2A = 127;         // 50% duty cycle en pin 11
}
