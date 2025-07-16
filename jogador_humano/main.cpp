/**
 * @file main.cpp
 * @brief Implementação de um jogo Campo Minado jogável por um humano.
 * @details Este programa utiliza a biblioteca SDL2 para criar uma interface gráfica
 * e permite que o usuário jogue Campo Minado com o mouse.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>

// === Constantes Globais do Jogo ===
const int CELL_SIZE = 30;      // Tamanho de cada célula em pixels
const int GRID_WIDTH = 10;     // Largura do tabuleiro em células
const int GRID_HEIGHT = 10;    // Altura do tabuleiro em células
const int NUM_MINES = 15;      // Número total de minas no tabuleiro
const int WINDOW_WIDTH = GRID_WIDTH * CELL_SIZE;   // Largura da janela em pixels
const int WINDOW_HEIGHT = GRID_HEIGHT * CELL_SIZE; // Altura da janela em pixels

// Gerador de números aleatórios global para o posicionamento das minas
std::random_device rd_global;
std::mt19937 gen_global(rd_global());

/**
 * @enum CellState
 * @brief Define os possíveis estados de uma célula no tabuleiro.
 */
enum CellState {
    HIDDEN,   // A célula está oculta.
    REVEALED, // A célula foi revelada.
    FLAGGED   // A célula foi marcada com uma bandeira.
};

/**
 * @struct Cell
 * @brief Representa uma única célula do tabuleiro.
 */
struct Cell {
    bool isMine = false;             // Verdadeiro se a célula contém uma mina.
    int neighboringMines = 0;        // Número de minas nas 8 células vizinhas.
    CellState state = HIDDEN;        // O estado atual da célula (oculta, revelada ou marcada).
};

/**
 * @struct Game
 * @brief Gerencia todo o estado e a lógica do jogo Campo Minado.
 */
struct Game {
    std::vector<std::vector<Cell>> grid; // Matriz 2D que representa o tabuleiro.
    bool gameOver = false;               // Flag que indica o fim do jogo por derrota.
    bool youWin = false;                 // Flag que indica o fim do jogo por vitória.
    bool firstClick = true;              // Flag para controlar a lógica especial do primeiro clique.

    /**
     * @brief Construtor da classe Game. Inicializa o grid com o tamanho definido.
     */
    Game() : grid(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH)) {}

    /**
     * @brief Prepara o tabuleiro para um novo jogo, resetando todas as células.
     */
    void initializeGrid() {
        gameOver = false;
        youWin = false;
        firstClick = true; // Prepara o jogo para um novo primeiro clique.

        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                grid[y][x] = Cell();
            }
        }
    }

    /**
     * @brief Inicia o jogo a partir do primeiro clique do usuário.
     * @details Esta função é chamada apenas uma vez por partida. Ela posiciona as minas
     * de forma que a área do primeiro clique seja segura e, em seguida,
     * revela a célula inicial.
     * @param startX A coordenada X do primeiro clique.
     * @param startY A coordenada Y do primeiro clique.
     */
    void startGameAt(int startX, int startY) {
        if (!firstClick) return; // Segurança extra para garantir que só execute uma vez.

        // 1. Posiciona as minas no tabuleiro.
        int placedMines = 0;
        while (placedMines < NUM_MINES) {
            int x = gen_global() % GRID_WIDTH;
            int y = gen_global() % GRID_HEIGHT;
            // Garante que a mina não seja colocada na área 3x3 ao redor do primeiro clique.
            bool isInSafeZone = (x >= startX - 1 && x <= startX + 1 && y >= startY - 1 && y <= startY + 1);
            if (!grid[y][x].isMine && !isInSafeZone) {
                grid[y][x].isMine = true;
                placedMines++;
            }
        }

        // 2. Calcula os números (minas vizinhas) para todas as células.
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if (grid[y][x].isMine) continue;
                grid[y][x].neighboringMines = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT && grid[ny][nx].isMine) {
                            grid[y][x].neighboringMines++;
                        }
                    }
                }
            }
        }

        firstClick = false; // Desativa a flag do primeiro clique.
        revealCell(startX, startY); // 3. Revela a célula inicial segura.
    }

    /**
     * @brief Revela uma célula e, se for vazia (0 minas vizinhas), revela suas vizinhas recursivamente.
     * @param x A coordenada X da célula a ser revelada.
     * @param y A coordenada Y da célula a ser revelada.
     */
    void revealCell(int x, int y) {
        // Condições de parada: célula fora do tabuleiro ou já revelada/marcada.
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT || grid[y][x].state != HIDDEN) {
            return;
        }

        grid[y][x].state = REVEALED;

        if (grid[y][x].isMine) {
            gameOver = true;
            return;
        }

        // Se a célula for um '0', chama a função para todas as vizinhas.
        if (grid[y][x].neighboringMines == 0) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    revealCell(x + dx, y + dy);
                }
            }
        }
        checkWinCondition();
    }

    /**
     * @brief Alterna o estado de uma célula entre oculta e marcada com bandeira.
     * @param x A coordenada X da célula.
     * @param y A coordenada Y da célula.
     */
    void toggleFlag(int x, int y) {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT || grid[y][x].state == REVEALED) return;
        if (firstClick) return; // Impede a colocação de bandeiras antes do início do jogo.

        if (grid[y][x].state == HIDDEN) {
            grid[y][x].state = FLAGGED;
        } else if (grid[y][x].state == FLAGGED) {
            grid[y][x].state = HIDDEN;
        }
    }

    /**
     * @brief Verifica se o jogador venceu a partida.
     * @details A condição de vitória é atingida quando todas as células que não são minas
     * foram reveladas.
     */
    void checkWinCondition() {
        int revealed_count = 0;
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if(grid[y][x].state == REVEALED) {
                    revealed_count++;
                }
            }
        }
        // Se o número de células reveladas for igual ao total de células menos o número de minas, o jogador vence.
        if (revealed_count == (GRID_WIDTH * GRID_HEIGHT) - NUM_MINES) {
            youWin = true;
        }
    }

    /**
     * @brief Renderiza o estado atual do tabuleiro na tela.
     * @param renderer O renderizador do SDL para desenhar.
     * @param font A fonte TTF para desenhar os textos (números e 'F').
     */
    void render(SDL_Renderer* renderer, TTF_Font* font) {
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                SDL_Rect cellRect = {x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE};

                // Desenha o fundo da célula
                if (grid[y][x].state == REVEALED) {
                    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
                }
                SDL_RenderFillRect(renderer, &cellRect);

                // Desenha o conteúdo da célula (número, mina ou bandeira)
                if (grid[y][x].state == REVEALED) {
                    if (grid[y][x].isMine) {
                        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                        SDL_RenderFillRect(renderer, &cellRect);
                    } else if (grid[y][x].neighboringMines > 0) {
                        SDL_Color textColor;
                        switch (grid[y][x].neighboringMines) {
                            case 1: textColor = {0, 0, 255}; break;
                            case 2: textColor = {0, 128, 0}; break;
                            case 3: textColor = {255, 0, 0}; break;
                            default: textColor = {128, 0, 128}; break;
                        }
                        std::string text = std::to_string(grid[y][x].neighboringMines);
                        SDL_Surface* textSurface = TTF_RenderText_Solid(font, text.c_str(), textColor);
                        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                        SDL_Rect textRect = {cellRect.x + (CELL_SIZE - textSurface->w) / 2, cellRect.y + (CELL_SIZE - textSurface->h) / 2, textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                        SDL_FreeSurface(textSurface);
                        SDL_DestroyTexture(textTexture);
                    }
                } else if (grid[y][x].state == FLAGGED) {
                    SDL_Surface* textSurface = TTF_RenderText_Solid(font, "F", {255, 0, 0});
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    SDL_Rect textRect = {cellRect.x + (CELL_SIZE - textSurface->w) / 2, cellRect.y + (CELL_SIZE - textSurface->h) / 2, textSurface->w, textSurface->h};
                    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                    SDL_FreeSurface(textSurface);
                    SDL_DestroyTexture(textTexture);
                }

                // Desenha a borda da célula
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &cellRect);
            }
        }

        // Se o jogo terminou, desenha uma mensagem de vitória ou derrota
        if (gameOver || youWin) {
            SDL_Color color = gameOver ? SDL_Color{255, 0, 0, 128} : SDL_Color{0, 255, 0, 128};
            std::string msg = gameOver ? "Voce Perdeu!" : "Voce Venceu!";
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_Rect overlay = {0, WINDOW_HEIGHT / 2 - 30, WINDOW_WIDTH, 60};
            SDL_RenderFillRect(renderer, &overlay);

            SDL_Surface* textSurface = TTF_RenderText_Solid(font, msg.c_str(), {255, 255, 255});
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Rect textRect = {(WINDOW_WIDTH - textSurface->w) / 2, (WINDOW_HEIGHT - textSurface->h) / 2, textSurface->w, textSurface->h};
            SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
            SDL_FreeSurface(textSurface);
            SDL_DestroyTexture(textTexture);
        }
    }
};

/**
 * @brief Função principal do programa.
 * @details Inicializa o SDL, cria a janela, gerencia o loop de eventos e renderização,
 * e limpa os recursos ao final.
 */
int main(int argc, char* argv[]) {
    // Inicialização das bibliotecas SDL2 e SDL2_ttf
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Criação da janela e do renderizador
    SDL_Window* window = SDL_CreateWindow("Campo Minado", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    // Carregamento da fonte (o caminho pode precisar ser ajustado)
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20);
    if (!font) {
        std::cerr << "Erro ao carregar fonte: " << TTF_GetError() << std::endl;
        return 1;
    }

    // Criação e inicialização do objeto do jogo
    Game game;
    game.initializeGrid();

    // Loop principal do jogo
    bool running = true;
    while (running) {
        SDL_Event event;
        // Processamento de eventos (input do usuário)
        while (SDL_PollEvent(&event)) {
            // Evento de fechar a janela
            if (event.type == SDL_QUIT) {
                running = false;
            }
            // Evento de clique do mouse
            if (event.type == SDL_MOUSEBUTTONDOWN && !game.gameOver && !game.youWin) {
                int x = event.button.x / CELL_SIZE;
                int y = event.button.y / CELL_SIZE;

                // Lógica para o clique esquerdo (revelar)
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Se for o primeiro clique, inicia o jogo de forma segura
                    if (game.firstClick) {
                        game.startGameAt(x, y);
                    } else {
                        game.revealCell(x, y);
                    }
                } 
                // Lógica para o clique direito (marcar/desmarcar bandeira)
                else if (event.button.button == SDL_BUTTON_RIGHT) {
                    game.toggleFlag(x, y);
                }
            }
            // Evento de pressionar uma tecla
            if (event.type == SDL_KEYDOWN) {
                // Se a tecla 'R' for pressionada, reinicia o jogo.
                if (event.key.keysym.sym == SDLK_r) {
                    game.initializeGrid();
                }
            }
        }

        // Seção de renderização do frame
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        game.render(renderer, font); // Chama a função de renderização do jogo

        SDL_RenderPresent(renderer); // Apresenta o frame desenhado na tela
    }

    // Liberação dos recursos do SDL ao fechar o programa
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}