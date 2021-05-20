#include <Arduino.h>

#define ENCODER_A 2
#define ENCODER_B 4
#define ENCODER_Z 3
#define MOTOR 5
#define ENABLE 6
#define PRESSURE_SENSOR A0
#define N 60     // Разрешение энкодера
#define START 20 // Номер оборота начала фиксации данных

bool start_flag = false;
bool send_flag = false;
volatile int num_revolutions = 0; // Счетчик оборотов
volatile int count = 0;           // Счетчик импульсов оборота
volatile int state = 0;           // Переменная хранящая статус вращения
volatile int pinAValue = 0;       // Переменные хранящие состояние пина импульсов энкодера
volatile int pinBValue = 0;
volatile int speed;
int **pressure_data; // Данные эксперимента
byte num_rep;        // Количество итераций эксперимента
volatile unsigned long start_t, end_t;

void establishContact()
{
  byte response;
  while (Serial.available() <= 0)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.print('C'); // Отправка символа для подтверждения соединения
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
    response = Serial.read(); // Обработка подтверждения соединения
    if (response == 'A')
    {
      return;
    }
  }
}

void A() // Прерывание по фазе A
{
  int pressure;
  pinAValue = digitalRead(ENCODER_A); // Получение состояние пинов A и B
  pinBValue = digitalRead(ENCODER_B);
  cli(); // Запрет обработки прерываний
  if (!pinAValue && pinBValue)
    state = 1; // Если при спаде линии А на линии B лог. единица, то вращение в одну сторону
  if (!pinAValue && !pinBValue)
    state = -1; // Если при спаде линии А на линии B лог. ноль, то вращение в другую сторону
  if (pinAValue && state != 0)
  {
    if (state == 1 && !pinBValue || state == -1 && pinBValue)
    { // Если на линии А снова единица, фиксируем шаг
      count += state;
      state = 0;
    }
  }
  if (start_flag == true && count < 60) // Чтение и сохранение данных давления
  {
    pressure = analogRead(PRESSURE_SENSOR);
    pressure_data[num_revolutions - START][count] = pressure;
  }
  sei(); // Разрешение обработки прерываний
}

void Z() // Прерывание по фазе Z
{
  int pressure;
  count = 0;
  num_revolutions++;
  if (num_revolutions == START)
    start_flag = true;
  start_t = millis();
  if (start_flag == true) // Чтение и сохранение давления в нулевой точке текущего оборота
  {
    pressure = analogRead(PRESSURE_SENSOR);
    pressure_data[num_revolutions - START][count] = pressure;
  }

  if (num_revolutions == START + num_rep) // Окончание эксперимента
  {
    end_t = millis();
    digitalWrite(ENABLE, LOW); // Остановка двигателя компрессора
    analogWrite(MOTOR, 0);
    speed = (num_rep * 60000) / (end_t - start_t);
    start_flag = false;
    send_flag = true;
  }
}

void init_data_array() // Инициализация массива данных
{
  for (int i = 0; i < num_rep; i++)
  {
    for (int j = 0; j < N; j++)
    {
      pressure_data[i][j] = 0;
    }
  }
}

void send_data() // Отправка данных эксперимента
{
  char *ptr;
  for (int i = 0; i < num_rep; i++)
  {
    for (int j = 0; j < N; j++)
    {
      ptr = (char *)&pressure_data[i][j];
      Serial.write(*ptr++); // Отправка младшего байта давления
      Serial.write(*ptr++); // Отправка старшего байта давления
    }
  }
  ptr = (char *)&speed;
  Serial.write(*ptr++); // Отправка младшего байта скорости
  Serial.write(*ptr++); // Отправка старшего байта скорости
}

void delete_data()
{ // Очищение массива данных
  for (int i = 0; i < num_rep; i++)
    delete pressure_data[i];
  delete[] pressure_data;
}

void setup()
{
  pinMode(PRESSURE_SENSOR, INPUT);
  ADCSRA |= (1 << ADPS2);                   // Присвоение единицы биту ADPS2  - коэффициент деления 16
  ADCSRA &= ~((1 << ADPS1) | (1 << ADPS0)); // Присвоение нулей битам ADPS1 и ADPS0
  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);
  pinMode(ENCODER_Z, INPUT);
  pinMode(MOTOR, OUTPUT);
  pinMode(ENABLE, OUTPUT);
  digitalWrite(ENABLE, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  attachInterrupt(1, Z, RISING); // Настройка прерывания индексного сигнала энкодера
  attachInterrupt(0, A, CHANGE); // Настройка прерывания сигнала A энкодера
  Serial.begin(9600);
  while (!Serial)
  {
    ;
  }
  establishContact(); // Ожидание подтверждения соединения
}
void loop()
{
  byte cmd, motor_value;
  if (Serial.available() >= 1)
  {
    // Чтение управляющей команды
    cmd = Serial.read();

    if (cmd == 49)
    {
      while (Serial.available() < 2)
      {
        ;
      }
      motor_value = Serial.read();
      num_rep = Serial.read();
      pressure_data = new int *[num_rep];
      for (int i = 0; i < num_rep; i++)
        pressure_data[i] = new int[N];
      init_data_array();
      digitalWrite(ENABLE, HIGH); // Запуск двигателя компрессора
      analogWrite(MOTOR, motor_value);
    }
  }
  if (send_flag == true)
  {
    send_data();
    delete_data();
    num_revolutions = 0;
    count = 0;
    speed = 0;
    send_flag = false;
  }
}
