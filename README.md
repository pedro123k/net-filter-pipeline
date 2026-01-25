# Ferramenta de filtragem digital via comunicação local

> Ferramenta para processamento digital de sinal configurável via JSON para filtragem utilizando comunicação em rede local UDP.  

Este projeto implementa um sistema para aplicar uma sequência de processos básicos de filtragens digitais a um sinal recebido e redistribuí-lo. As configurações gerais de comunicação UDP e as especificações dos filtros aplicados são feitas através de um arquivo JSON. O projeto também contempla duas ferramentas implementadas em Python para visualizar o funcionamento do sistema.

O principal objetivo da ferramenta é disponibilizar na rede local um *back-end* capaz de realizar tarefas de DSP em sinais locais e enviar o resultado do processamento para outros componentes locais. O uso de comunicação por datagramas é um esforço para mitigar o custo em performance comum a protocolos de comunicação mais sofisticados.

## 🔍 Visão Geral 

O sistema utiliza a modelagem do maravilhoso [Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) para implementar filtros IIR de segunda ordem passa-alta, passa-baixa, passa-faixa e notch. Filtros passa-alta e passa-baixa de ordens superiores 2N são viabilizados através de cascata de filtros biquadráticos. Filtros notch e passa-faixa estão restritos à ordem 2 neste projeto.

A comunicação UDP trabalha com blocos de dados de 128 valores `float`. O tamanho foi escolhido para minimizar a possibilidade de fragmentação de informações durante a comunicação. Adicionalmente, o *back-end* deve receber no pacote, juntamente com o bloco de dados do sinal, um inteiro de sequência para diminuir o efeito de reordenações aleatórias da rede na chegada dos pacotes, e um inteiro referente a uma porta de saída para a qual o sinal será redistribuído.

O pacote esperado pelo sistema possui o seguinte formato: ```seq (64 bits)``` ```port (16 bits)``` ```data ( 128 * 32 bits)```

Para lidar com a perda de pacotes, o sistema trabalha com uma política de *concealment* que pode ser definida no arquivo de configurações. Há três políticas básicas disponíveis: repetir a última saída, repetir a última saída com atenuação e sinal nulo. O sistema não trabalha com o conceito de retransmissão de pacotes por questões de performance.

## ▶️ Como Executar (Windows)

### 1. Baixe e salve o executável em *Release*
### 2. Crie um arquivo JSON com suas configurações na mesma pasta
### 3. Execute no Terminal dentro da pasta
```bash
.\net-filter-pipeline_x86_64.exe minha_configuracao.json
```
Adicionalmente, é possível passar um parâmetro ```--dump-coeffs nome_arquivo.txt``` para salvar os coeficientes dos filtros para a visualização da resposta em frequência.

### 4. Clone o repositório e instale as dependências para as ferramentas em Python (opcional)
```bash
# Em um terminal separado
git clone https://github.com/pedro123k/net-filter-pipeline.git  
cd net-filter-pipeline
python -m venv ./venv
.\.venv\Scripts\activate
pip install -r requirements.txt
```
### 5. Execute o Gerador de Sinais (opcional)
```bash 
python .\tools\signal_gen.py
```

### 6. Execute o Visualizador de Sinais em um terminal separado (opcional)
```bash
# Dentro da pasta net-filter-pipeline
python .\tools\signal_viz.py
```

## ⚙️ Estrutura do arquivo de configurações em JSON
```json
{
    "udp-parms": 
    {
        "server-port": 55555,
        "samp-freq": 2000,
        "client-addrv4": "127.0.0.1",
        "concealment-policy": "FADE_LAST_GOOD"
    },
    "pipeline": [
        {
            "type": "gain",
            "gain": 2
        },
        {
            "type": "high-pass",
            "cut-freq": 200,
            "order": 4
        },
        {
            "type": "notch",
            "cut-freq": 60,
            "BW": 1
        }
    ]
}
```
- ```server-port```: porta responsável por receber os sinais de entrada
- ```samp-freq```: frequência de amostragem
- ```client-addrv4```: endereço IPv4 responsável por transmitir os dados processados
- ```concealment-policy (REPEAT_LAST_GOOD | FADE_LAST_GOOD | ALL_ZERO) ```: política de *concealment*
- ```type ('low-pass' | 'high-pass' | 'notch' | 'band-pass' | 'gain' )```: tipo do filtro / elemento
- ```gain```: ganho do elemento do tipo ganho
- ```cut-freq```: frequência de corte
- ```order```: ordem do filtro (filtros passa-baixa e passa-alta)
- ```Q```: razão entre frequência central e largura de banda
- ```BW```: largura de banda (filtros notch e passa-faixa), em oitavas

## 🛠️ Build

### 🪟 Windows 

#### 1. Instale o MSYS2 UCRT64
#### 2. Abra o terminal do MSYS2 UCRT64
#### 3. Execute
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-pkg-config
```
#### 4. Clone o repositório
```bash
git clone https://github.com/pedro123k/net-filter-pipeline.git  
cd net-filter-pipeline
```
#### 5. Execute
```bash
cmake -S . -B build -G Ninja
cmake --build build --target app
```

## 📁 Estrutura do Projeto

```text
.
├── configs/            # Arquivos de configurações JSON para o sistema
├── include/            # Headers da biblioteca do projeto
├── src/                # Implementação do código-fonte
├── tools/              # Ferramentas em Python para geração e visualização de sinais básicos
├── requirements.txt
├── README.md
└── .gitignore
```

## 📸 Capturas do Projeto
<img src=imgs/img1.gif width="600">
<img src=imgs/img2.gif width="600">
<img src=imgs/img3.png width="300">

## 🚧 Status do Projeto

Implementação inicial finalizada.  
O projeto encontra-se funcional, mas ainda pouco escalável e sujeito a perdas periódicas.










