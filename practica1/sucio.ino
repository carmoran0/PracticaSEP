const int botonA = 8;
const int botonB = 9;
const int ledEntrega = 7;
const int ledError = 6;
const int detectorRecogida = 10;
const int potenciometro = 1;
const int ledsBinario0 = 5;
const int ledsBinario1 = 4;
const int ledsBinario2 = 3;
const int ledsBinario3 = 2;

enum Estado {cuantico, idle, recogida, comprar, tecnico, pretecnico, postecnico};
enum EstadoBoton{PULSADO, SUELTO, TR_PULS, TR_SOLT};
Estado estado = cuantico;
EstadoBoton estadoBotonA = SUELTO, estadoBotonB = SUELTO, estadoBotonPuente = SUELTO;
unsigned long t_ini = 0, t_initecnico = 0;

bool bloq = 0;
int saldo=0, precioProducto=5, compartimentos[3] = {3, 3, 3};

unsigned long tiempoParpadeo = 0;
bool parpadeando = false;
int contadorParpadeo = 0;
int pinParpadeo = -1;

unsigned long t_iniEliminarSaldo = 0;
bool saldoEliminado = false;

void setup() {
  // put your setup code here, to run once
    delay(500);
    pinMode(2,OUTPUT);
    pinMode(3,OUTPUT);
    pinMode(4,OUTPUT);
    pinMode(5,OUTPUT);
    pinMode(6,OUTPUT); // LED ERROR
    pinMode(7,OUTPUT); // LED TRANS
    pinMode(8,INPUT_PULLUP); // SI USO PULLUP PORQUE no   HE PUESTO LAS RESISTENCIAS YO
    pinMode(9,INPUT_PULLUP);
    pinMode(10,INPUT_PULLUP);
    Serial.begin(9600);
}
/*
https://docs.arduino.cc/retired/hacking/software/PortManipulation/ DOC AQUI
(1 << pin): Es una operación de desplazamiento de bits. 
Aquí, el número 1 se desplaza a la izquierda pin veces. 
Esto crea un número binario en el que solo el bit en la posición pin es 1, 
y todos los demás bits son 0. Por ejemplo, si pin es 3, 
entonces (1 << pin) resulta en 00001000 en binario.*/

void swagDigitalWrite(int pin, bool valor) {
  if (pin >= 2 && pin <= 7) {
    if (valor) {
      PORTD |= (1 << pin); // Se usa OR para poder sobreescribir el 1 en el 0
    } else {
      PORTD &= ~(1 << pin); // Se usa AND para poder sobreescribir el 0 en el 1
    }
  } else if (pin >= 8 && pin <= 13) {
    if (valor) {
      PORTB |= (1 << (pin - 8)); 
    } else {
      PORTB &= ~(1 << (pin - 8));
    }
  }
}

bool swagDigitalRead(int pin) {
  if (pin >= 2 && pin <= 7) {
    return PIND & (1 << pin); // Lee el bit correspondiente en PIND
  } else if (pin >= 8 && pin <= 13) {
    return PINB & (1 << (pin - 8)); // Ajuste para el rango de pines 8-13
  }
  return false; // Retorna false si el pin no está en el rango válido
}

int swagAnalogRead(int pin) {

// file:///C:/Users/carmo/Desktop/Sistemas%20electronicos%20programables/practica1/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf
// Página 206 IMPORTANTE 211 tmabien=??

  // Configurar el pin como entrada analógica y activar el bit 7
  ADMUX = (ADMUX & 0xF0) | (pin & 0x0F) 
  | (0 << 7)| (1 << 6); // Hago mascara para que no se modifiquen los bits, 6 es REFS0 y 7 es REF1
  // Eso anterior en la página 218
  // Serial.println(ADMUX);
  // Iniciar la conversión
  ADCSRA |= (1 << ADSC); // bit 6 en ADCSRA es ADSC, ver pág 218
  // Esperar a que la conversión termine
  while (ADCSRA & (1 << ADSC));
    //Serial.println("Esperando a que termine la conversión ADC...");
    //Serial.println(ADSC);

  // Retornar el valor leído
  return ADC;
}

EstadoBoton gestionBoton(EstadoBoton estadoAnteriorBoton, bool lectura){
  if(lectura == LOW){ // PULSADO
      if(estadoAnteriorBoton == PULSADO ||
         estadoAnteriorBoton == TR_PULS)
          return PULSADO;
      else
          return TR_PULS;
  }
  else{ // SUELTO
      if(estadoAnteriorBoton == PULSADO ||
         estadoAnteriorBoton == TR_PULS)
          return TR_SOLT;
      else
          return SUELTO;
  }
}

int seleccionarCompartimento() {
  int maxIndex = 0;
  for (int i = 1; i < 3; i++) {
    if (compartimentos[i] > compartimentos[maxIndex]) {
      maxIndex = i;
    }
  }
  return maxIndex;
}

void iniciarParpadeo(int pin) {
  parpadeando = true;
  contadorParpadeo = 0;
  pinParpadeo = pin;
  tiempoParpadeo = millis();
}

void gestionarParpadeo() {
  if (parpadeando) {
    if (millis() - tiempoParpadeo >= 200) {
      tiempoParpadeo = millis();
      if (contadorParpadeo < 6) {
        swagDigitalWrite(pinParpadeo, !swagDigitalRead(pinParpadeo));
        contadorParpadeo++;
      } else {
        parpadeando = false;
        swagDigitalWrite(pinParpadeo, LOW);
      }
    }
  }
}

void loop() {
// put your main code here, to run repeatedly:
  estadoBotonA = gestionBoton(estadoBotonA, swagDigitalRead(botonA));
  estadoBotonB = gestionBoton(estadoBotonB, swagDigitalRead(botonB));
  estadoBotonPuente = gestionBoton(estadoBotonPuente, swagDigitalRead(detectorRecogida));
  //Serial.println(estadoBotonA);
  //Serial.print(estadoBotonB);
  //Serial.print(estadoBotonPuente);
  //Serial.println();
  gestionarParpadeo();
  switch (estado){
  case cuantico:
    t_ini = millis();
    estado = idle;
    bloq = 0;
    saldoEliminado = false;
    Serial.println();
    Serial.print("PROGRAMA COMIENZA EN EL MS: ");
    Serial.println(t_ini);
    Serial.print("El precio del producto es de: ");
    Serial.print(precioProducto);
    Serial.println();
    break;


  case idle:
    // Serial.println(estadoBotonA);
    // Serial.print(estadoBotonB);
    if (estadoBotonA == TR_SOLT && estadoBotonB == !PULSADO && !saldoEliminado) {
      saldo += swagAnalogRead(potenciometro) * 16 / 1024;
      Serial.print(saldo);
      Serial.print(" EUR actualmente en el saldo\n");
    }

    else if (estadoBotonA == PULSADO && estadoBotonB == PULSADO) { //TECNICO
      if (bloq == 0) {
        t_initecnico = millis();
        bloq = 1;
      }
      if (bloq == 1 && millis() - t_initecnico >= 2000) {
        estado = pretecnico; // Cambiamos a un estado intermedio para evitar activar funciones al soltar
        Serial.println("MODO TECNICO ACTIVADO");
      }

    } else if (estadoBotonB == TR_SOLT && estadoBotonA == !PULSADO) { //COMPRAR
      if (saldo >= precioProducto) {
        int compartimentoSeleccionado = seleccionarCompartimento();
        if (compartimentos[compartimentoSeleccionado] > 0) {
          saldo -= precioProducto;
          compartimentos[compartimentoSeleccionado]--;
          Serial.print("Producto entregado del compartimento: ");
          Serial.println(compartimentoSeleccionado + 1);
          Serial.println("POR FAVOR RETIRE EL PRODUCTO ");
          for (int i = 0; i < 3; i++) {
            Serial.print("[Comp. ");
            Serial.print(i + 1);
            Serial.print("]: ");
            Serial.println(compartimentos[i]);
          }
          iniciarParpadeo(ledEntrega);
          estado = recogida; // Cambiar al estado recogida para esperar la recogida
        } else {
          Serial.println("El compartimento/la máquina está vacía.");
          iniciarParpadeo(ledError);
        }
      } else {
        Serial.println("Saldo insuficiente");
        iniciarParpadeo(ledError);
      }
    } else if (estadoBotonA == PULSADO && estadoBotonB == SUELTO) { // ELIMINAR SALDO
      if (bloq == 0) {
        t_iniEliminarSaldo = millis();
        bloq = 1;
      }
      if (bloq == 1 && millis() - t_iniEliminarSaldo >= 5000) {
        saldo = 0;
        Serial.println("Saldo eliminado");
        iniciarParpadeo(ledError);
        saldoEliminado = true;
        bloq = 0;
      }
    } else {
      bloq = 0; // Resetear el bloqueo si se suelta algún botón
      saldoEliminado = false;
    }
    break;

  case recogida:
    if (estadoBotonPuente == PULSADO) {
      Serial.println("Producto recogido");
      estado = idle;
    }
    break;

  case pretecnico: // Estado intermedio para evitar activar funciones al soltar los botones
    if (estadoBotonA == SUELTO && estadoBotonB == SUELTO) {
      estado = tecnico;
      bloq = 0; // Resetear el bloqueo
    }
    break;
    
  case tecnico:
    if (estadoBotonA == PULSADO && estadoBotonB == PULSADO) {
      // Si ambos botones se pulsan a la vez, preparamos para salir del modo técnico
      if (bloq == 0) {
        t_initecnico = millis();
        bloq = 1;
      }
      if (bloq == 1 && millis() - t_initecnico >= 2000) {
        estado = postecnico; // Cambiamos a un estado intermedio para evitar activar funciones al soltar
        bloq = 0;
        Serial.println("MODO TECNICO DESACTIVADO");
      }
    } else if (estadoBotonA == TR_SOLT && estadoBotonB != PULSADO) {
      precioProducto = swagAnalogRead(potenciometro) * 16 / 1024;
      Serial.print("Nuevo precio del producto: ");
      Serial.println(precioProducto);
    } else if (estadoBotonB == TR_SOLT && estadoBotonA != PULSADO) { 
      int compartimentoSeleccionado = (swagAnalogRead(potenciometro) * 16 / 1024);
      if (compartimentoSeleccionado >= 0 && compartimentoSeleccionado < 3) {
        compartimentos[compartimentoSeleccionado]++;
        Serial.print("Añadida una unidad al compartimento: ");
        Serial.println(compartimentoSeleccionado + 1);
        for (int i = 0; i < 3; i++) {
          Serial.print("[Comp. ");
          Serial.print(i + 1);
          Serial.print("]: ");
          Serial.println(compartimentos[i]);
        }
      } else {
        Serial.println("Compartimento no válido");
      }
    } else {
      bloq = 0; // Resetear el bloqueo si se sueltan los botones
    }
    break;
    
  case postecnico: // Estado intermedio para evitar activar funciones al soltar los botones
    if (estadoBotonA == SUELTO && estadoBotonB == SUELTO) {
      estado = idle;
      bloq = 0; // Resetear el bloqueo
    }
    break;
  }

  int valor = swagAnalogRead(potenciometro) * 16 / 1024;
  bool valorBinario[4]; // Array para almacenar el valor binario
  // Convertir el valor a binario manualmente
  for (int i = 3; i >= 0; i--) {
    valorBinario[i] = valor % 2;
    valor /= 2;
  }
  // Mostrar el valor en los LEDs
  swagDigitalWrite(ledsBinario3, valorBinario[3]);
  swagDigitalWrite(ledsBinario2, valorBinario[2]);
  swagDigitalWrite(ledsBinario1, valorBinario[1]);
  swagDigitalWrite(ledsBinario0, valorBinario[0]);

  /*
  //DebugBinario
  Serial.print(swagAnalogRead(1) * 16 / 1024);
  Serial.print(" = ");
  Serial.print(valorBinario[0]);
  Serial.print(valorBinario[1]);
  Serial.print(valorBinario[2]);
  Serial.println(valorBinario[3]);
  */
  
  /*
  //Debugbotones
  Serial.print("PIN 8:");
  Serial.print(swagDigitalRead(8));
  Serial.print(digitalRead(8));
  Serial.print(" : ");
  Serial.println(byte(PINB));
  delay(1000);

  Serial.print("PIN 9:");
  Serial.print(swagDigitalRead(9));
  Serial.print(digitalRead(9));
  Serial.print(" : ");
  Serial.println();
  delay(1000);
  */

  
  // DebugADC/potenciometro
  // Serial.print("ADC");
  // Serial.println(ADC);
  // Serial.print("analogo");
  // Serial.println(analogRead(potenciometro));
  // Serial.print("SWAGanalogo");
  // Serial.println(swagAnalogRead(potenciometro));
  // delay(1000);
}
