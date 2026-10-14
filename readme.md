# 🦴 StraightBack

> Dispositivo vestível que monitora a postura da coluna em tempo real, avisa com vibração quando você relaxa demais e mostra o histórico num painel web.

![Projete 2025](https://img.shields.io/badge/Projete-2025-5B6EE1)
![ESP32](https://img.shields.io/badge/ESP32-Arduino-00979D?logo=arduino&logoColor=white)
![Firebase](https://img.shields.io/badge/Firebase-Auth-FFCA28?logo=firebase&logoColor=black)
![Chart.js](https://img.shields.io/badge/Chart.js-4-FF6384?logo=chartdotjs&logoColor=white)


## 🏆 Sobre o projeto

O StraightBack foi desenvolvido para a **45ª Projete (2025)** e foi premiado como **Melhor Projeto da Categoria “Saúde e Bem-Estar” do 2º ano**.

Passar horas sentado, curvado sobre o computador ou o celular, é uma das principais causas de dor nas costas e no pescoço. O StraightBack mede a inclinação da **cervical** e da **lombar** o tempo todo, avisa na hora (e discretamente) quando a postura errada se prolonga e registra quanto tempo você passou torto.

## ⚙️ Como funciona

1. **Calibração.** Na postura correta, o usuário segura o botão de calibração por 3 s. O dispositivo coleta a posição durante 12 s e define o ângulo neutro e os limites aceitáveis de cada região, salvos na memória não volátil (NVS) do ESP32.
2. **Leitura contínua.** Dois sensores **MPU6050** (acelerômetro + giroscópio), um na cervical e outro na lombar, são lidos em barramentos I²C separados. O filtro **Madgwick** funde os dados e calcula o ângulo (*roll*) de cada região.
3. **Detecção de postura.** Se o ângulo sai da faixa calibrada e continua assim além do tempo limite, o dispositivo acende um LED e aciona os **motores vibracall**. Se o usuário não corrige, o aviso se repete e passa a vibrar em todas as regiões.
4. **Sentado ou em pé.** A inclinação da lombar indica se o usuário está sentado, e o limite da cervical se ajusta a isso.
5. **Envio dos dados.** Com Wi-Fi, o ESP32 envia a cada minuto um JSON com o tempo acumulado de postura incorreta, os limites calibrados e o estado atual (sentado, calibrado).
6. **Painel web.** O usuário entra com e-mail e senha e vê o tempo de postura incorreta por região, os parâmetros calibrados e o estado atual, atualizados a cada 30 s.

### 📶 Configuração de Wi-Fi sem reprogramar

Sem credenciais salvas, o ESP32 cria uma rede própria (**StraightBack**) com um **captive portal**. O usuário escolhe sua rede, digita a senha e o dispositivo salva e conecta sozinho. Se a conexão cair, ele tenta reconectar periodicamente e reabre o portal se necessário.

### 🔘 Botões

| Ação | Resultado |
|---|---|
| Segurar o botão de calibração por 3 s e manter a postura correta por 12 s | Calibra o dispositivo |
| Segurar o botão de reset por 5 s | Apaga os parâmetros salvos |

## 🧰 Tecnologias

| Parte | Stack |
|---|---|
| Firmware | C++ (Arduino framework) no **ESP32**, MadgwickAHRS, ArduinoJson, Preferences (NVS), WebServer + DNSServer |
| Sensores / atuadores | 2× MPU6050, motores vibracall, LED, 2 botões |
| Backend | Endpoint HTTP que recebe o JSON do dispositivo |
| Painel web | HTML, CSS e JavaScript puro, Chart.js, Firebase Authentication |

## 📁 Estrutura

```
StraightBack/
├── firmware/
│   └── codigo_o_final/
│       └── codigo_o_final.ino   # código do ESP32
└── site/
    ├── index.html               # painel (login, dashboard, parâmetros)
    ├── css/
    └── js/
```

## 🔌 Hardware

| Componente | Pino / endereço |
|---|---|
| MPU6050 cervical | I²C 0x68, SDA 18 / SCL 19 |
| MPU6050 lombar | I²C 0x69, SDA 21 / SCL 22 |
| Vibracall cervical / lombar / torácica | GPIO 5 / 12 / 14 |
| LED | GPIO 13 |
| Botão de calibração / reset | GPIO 4 / 2 |

## 🚀 Como rodar

### Firmware
1. Instale a **Arduino IDE** com o suporte a placas ESP32.
2. Instale as bibliotecas: `MadgwickAHRS` e `ArduinoJson`.
3. Em `serverUrl`, coloque o endereço do seu endpoint.
4. Selecione a placa ESP32 e faça o upload de `codigo_o_final.ino`.

### Painel web
1. Crie um projeto no **Firebase** e ative o login por e-mail/senha.
2. Em `site/js/index.js`, troque `firebaseConfig` pelos dados do seu projeto e ajuste `DATA_ENDPOINT`.
3. Abra `site/index.html` por um servidor estático (por exemplo `npx serve site`).

## 👨‍💻 Equipe

<!-- Ajuste os links de cada integrante -->
- **Layon Rubens Motta de Freitas** 
- **Cauã Eduardo Silva** 
- **João Pedro Ferraz de Oliveira** 

