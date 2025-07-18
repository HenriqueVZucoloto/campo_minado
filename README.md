# Campo Minado com IA
Este projeto, desenvolvido em C++ com a biblioteca gráfica SDL2, explora diferentes abordagens para a resolução do clássico jogo Campo Minado. Ele é composto por três módulos distintos: uma versão jogável para humanos, um agente de IA com lógica pré-definida e um agente de IA que aprende e evolui através de um Algoritmo Genético.

## Estrutura do Projeto

O repositório está organizado de forma modular para separar claramente cada implementação:

```
/
├── 📂 agente_genetico/      # Contém a IA baseada em Algoritmo Genético
│   └── main.cpp
├── 📂 agente_hardcoded/    # Contém a IA com regras lógicas pré-definidas
│   └── main.cpp
├── 📂 jogador_humano/      # Contém a versão clássica e jogável do Campo Minado
│   └── main.cpp
└── 📜 README.md             # Este arquivo
```

## Funcionalidades Principais

* **Jogo Interativo:** Uma implementação completa e funcional do Campo Minado para um jogador humano.
* **Agente Lógico (Hardcoded):** Uma IA que utiliza uma estratégia de duas camadas:
    1.  **Regras Determinísticas:** Aplica a lógica básica do Campo Minado para jogadas 100% seguras.
    2.  **Análise Probabilística:** Quando a lógica simples não encontra jogadas, o agente analisa todas as configurações possíveis na "fronteira" do jogo para fazer um "chute inteligente" baseado na menor probabilidade de encontrar uma mina.
* **Agente Evolutivo (Genético):** Uma IA que evolui do zero.
    * Utiliza um **Algoritmo Genético** para evoluir um "cérebro" composto por 150* regras.
    * A performance (`fitness`) é avaliada contra um banco de 200* cenários de teste fixos para garantir justiça e consistência.
    * O progresso do treinamento é salvo, permitindo que a evolução continue de onde parou.
  
    *esses valores podem ser alterados antes de iniciar o treinamento

## Pré-requisitos e Instalação

Para compilar e executar qualquer um dos módulos, você precisará de:
* Um compilador C++ (g++, Clang, etc.)
* A biblioteca de desenvolvimento do `SDL2`
* A biblioteca de desenvolvimento do `SDL2_ttf` (para renderização de fontes)

### Comandos de Instalação

* **Linux (Debian/Ubuntu):**
    ```bash
    sudo apt-get install build-essential libsdl2-dev libsdl2-ttf-dev
    ```
* **Windows (usando MSYS2/MinGW):**
    ```bash
    pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf
    ```
* **macOS (usando Homebrew):**
    ```bash
    brew install sdl2 sdl2_ttf
    ```

---

## Compilação e Execução

**⚠️ AVISO IMPORTANTE:** Todos os arquivos `.cpp` contêm um caminho para uma fonte (`.ttf`). Você **PRECISA** alterar este caminho para um que seja válido no seu sistema operacional antes de compilar.

* Exemplo Windows: `"C:\\Windows\\Fonts\\arial.ttf"`
* Exemplo macOS: `"/System/Library/Fonts/Supplemental/Arial.ttf"`

### 1. Módulo: Jogador Humano

Este módulo permite que você jogue Campo Minado.

```bash
# 1. Navegue até a pasta
cd jogador_humano

# 2. Compile o código
g++ main.cpp -o jogo_manual -std=c++17 -lSDL2 -lSDL2_ttf

# 3. Execute
./jogo_manual
```
**Controles:**
* **Clique Esquerdo:** Revelar uma casa.
* **Clique Direito:** Colocar/Remover uma bandeira.
* **Tecla 'R':** Reiniciar a partida.

### 2. Módulo: Agente Hardcoded

Este módulo executará a IA com regras pré-definidas. O terminal mostrará o "raciocínio" do agente.

```bash
# 1. Navegue até a pasta
cd agente_hardcoded

# 2. Compile o código
g++ main.cpp -o agente_hardcoded -std=c++17 -lSDL2 -lSDL2_ttf

# 3. Execute
./agente_hardcoded
```
**Controles:**
* **Tecla 'R':** Iniciar uma nova partida para o agente resolver.

### 3. Módulo: Agente Genético

Este módulo iniciará o processo de treinamento da IA evolutiva.

```bash
# 1. Navegue até a pasta
cd agente_genetico

# 2. Compile o código
g++ main.cpp -o agente_genetico -std=c++17 -O2 -lSDL2 -lSDL2_ttf -lpthread

# 3. Execute
./agente_genetico
```
**Funcionamento:**
* Ao ser executado pela primeira vez, ele criará dois arquivos:
    * `fixed_games.dat`: O banco com 200 cenários de teste.
    * `populacao_regras.dat`: O "save" da sua população de IAs.
* O terminal exibirá o progresso de cada geração (melhor fitness, taxa de vitórias).
* O treinamento pode ser interrompido (`Ctrl+C`) e retomado. O programa carregará o `populacao_regras.dat` e continuará de onde parou.
* Para iniciar um treinamento do zero, simplesmente delete os dois arquivos `.dat`.


**Demonstração**

https://github.com/user-attachments/assets/1d381a52-aef3-4598-beea-bbb7c53e261d

