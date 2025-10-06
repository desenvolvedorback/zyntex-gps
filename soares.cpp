// --- Configurações do Hardware ---
const int PINO_TENSAO = A0;      // Pino Analógico conectado ao divisor de tensão
const int RELE_PIN = 7;          // Pino Digital conectado ao pino IN/SIG do relé

// --- Configurações do Relé ---
// Ajuste estas linhas se o seu módulo for ativado por HIGH (mais raro)
const int RELE_LIGAR = LOW;      // Sinal para LIGAR o relé
const int RELE_DESLIGAR = HIGH;  // Sinal para DESLIGAR o relé

// --- Configurações da Tensão ---
// Tensão MÁXIMA que o divisor de tensão consegue ler (Exemplo: 15V para R1=20k, R2=10k)
const float MAX_TENSAO_LIDA = 15.0; 

// Tensão LIMITE de acionamento (Ajuste este valor para sua aplicação)
const float LIMITE_DE_ACIONAMENTO = 12.5; 

void setup() {
  // Configura o pino do relé como SAÍDA
  pinMode(RELE_PIN, OUTPUT);
  
  // Garante que o relé esteja DESLIGADO no início (segurança)
  digitalWrite(RELE_PIN, RELE_DESLIGAR);
  
  // Inicia a comunicação serial para monitoramento e debug
  Serial.begin(9600);
  Serial.println("Monitor de Tensao com Rele Iniciado.");
}

void loop() {
  // 1. Leitura do Valor Analógico (0 a 1023)
  int valorAnalogico = analogRead(PINO_TENSAO);

  // 2. Conversão para Tensão Real em Volts
  // 5.0 é a tensão de referência do Arduino (Vcc)
  // 1023 é o valor máximo da leitura analógica
  // MAX_TENSAO_LIDA é a sua tensão máxima calculada com o divisor de tensão
  float tensaoLida = valorAnalogico * (MAX_TENSAO_LIDA / 1023.0);

  // 3. Lógica de Acionamento
  if (tensaoLida > LIMITE_DE_ACIONAMENTO) {
    // Tensão excedeu o limite: LIGA o relé
    digitalWrite(RELE_PIN, RELE_LIGAR);
    Serial.print("TENSAO ALTA: ");
    Serial.print(tensaoLida);
    Serial.println("V. Rele LIGADO.");
  } else {
    // Tensão está abaixo do limite: DESLIGA o relé
    digitalWrite(RELE_PIN, RELE_DESLIGAR);
    Serial.print("Tensao normal: ");
    Serial.print(tensaoLida);
    Serial.println("V. Rele DESLIGADO.");
  }

  // Pequena pausa para estabilizar a leitura e a Serial
  delay(500);
}
