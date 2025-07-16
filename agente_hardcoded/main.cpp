/**
 * @file main.cpp
 * @brief Implementação de um agente de IA (Inteligência Artificial) para resolver o jogo Campo Minado.
 * @details O agente utiliza um conjunto de regras lógicas pré-definidas ("hardcoded") para tomar decisões.
 * A estratégia inclui regras determinísticas básicas e uma análise probabilística por força bruta
 * para situações de impasse. A interface gráfica é renderizada com a biblioteca SDL2.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <thread>
#include <chrono>
#include <map>
#include <algorithm>
#include <iomanip>

// === Constantes Globais do Jogo ===
const int CELL_SIZE = 30;      // Tamanho de cada célula em pixels
const int GRID_WIDTH = 10;     // Largura do tabuleiro em células
const int GRID_HEIGHT = 10;    // Altura do tabuleiro em células
const int NUM_MINES = 15;      // Número total de minas no tabuleiro
const int WINDOW_WIDTH = GRID_WIDTH * CELL_SIZE;   // Largura da janela em pixels
const int WINDOW_HEIGHT = GRID_HEIGHT * CELL_SIZE; // Altura da janela em pixels

// --- Estruturas, Enums e Globais ---

// Alias para um par de inteiros, usado para representar coordenadas (x, y).
using CellCoord = std::pair<int, int>;

/**
 * @enum MoveResult
 * @brief Descreve o resultado da análise de força bruta da IA.
 */
enum class MoveResult {
    GUARANTEED_MOVE_FOUND, // Uma jogada 100% segura (mina ou casa livre) foi encontrada e executada.
    NO_GUARANTEED_MOVE,    // Nenhuma jogada segura foi encontrada, mas foi calculado um chute probabilístico.
    FAILED                 // A análise falhou (ex: fronteira muito grande ou nenhuma solução válida).
};

/**
 * @enum CellState
 * @brief Define os possíveis estados de uma célula no tabuleiro.
 */
enum CellState { HIDDEN, REVEALED, FLAGGED };

/**
 * @struct Cell
 * @brief Representa uma única célula do tabuleiro.
 */
struct Cell {
    bool isMine = false;
    int neighboringMines = 0;
    CellState state = HIDDEN;
};

// Gerador de números aleatórios global.
std::random_device rd_global;
std::mt19937 gen_global(rd_global());

/**
 * @struct Game
 * @brief Gerencia todo o estado e a lógica do jogo Campo Minado.
 */
struct Game {
    std::vector<std::vector<Cell>> grid; // Matriz 2D que representa o tabuleiro.
    bool gameOver = false;               // Flag que indica o fim do jogo por derrota.
    bool youWin = false;                 // Flag que indica o fim do jogo por vitória.
    bool firstMoveMade = false;          // Flag para controlar a lógica especial do primeiro movimento.

    /**
     * @brief Construtor da classe Game. Inicializa o grid com o tamanho definido.
     */
    Game() : grid(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH)) {}

    /**
     * @brief Reseta o tabuleiro para um estado inicial vazio, pronto para um novo jogo.
     */
    void initializeGrid() {
        gameOver = false;
        youWin = false;
        firstMoveMade = false;
        for (int y = 0; y < GRID_HEIGHT; ++y) for (int x = 0; x < GRID_WIDTH; ++x) grid[y][x] = Cell();
    }

    /**
     * @brief Inicia o jogo a partir de uma coordenada segura.
     * @details Popula o tabuleiro com minas, garantindo que a área ao redor do ponto inicial esteja livre.
     * @param startX A coordenada X do ponto de partida.
     * @param startY A coordenada Y do ponto de partida.
     */
    void startGameAt(int startX, int startY) {
        if (firstMoveMade) return;
        
        // 1. Posiciona as minas no tabuleiro, evitando a "safe zone" 3x3 inicial.
        int placedMines = 0;
        while (placedMines < NUM_MINES) {
            int x = gen_global() % GRID_WIDTH;
            int y = gen_global() % GRID_HEIGHT;
            bool isInSafeZone = (x >= startX - 1 && x <= startX + 1 && y >= startY - 1 && y <= startY + 1);
            if (!grid[y][x].isMine && !isInSafeZone) {
                grid[y][x].isMine = true;
                placedMines++;
            }
        }
        
        // 2. Calcula os números (minas vizinhas) para todas as células do tabuleiro.
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if (grid[y][x].isMine) continue;
                grid[y][x].neighboringMines = 0;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT && grid[ny][nx].isMine) {
                        grid[y][x].neighboringMines++;
                    }
                }
            }
        }
        
        firstMoveMade = true;
        // 3. Revela a célula inicial segura.
        revealCell(startX, startY);
    }

    /**
     * @brief Revela uma célula e, se for vazia (0), revela suas vizinhas recursivamente.
     * @param x A coordenada X da célula.
     * @param y A coordenada Y da célula.
     */
    void revealCell(int x, int y) {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT || grid[y][x].state != HIDDEN) return;
        grid[y][x].state = REVEALED;
        if (grid[y][x].isMine) { gameOver = true; return; }
        if (grid[y][x].neighboringMines == 0) {
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                revealCell(x + dx, y + dy);
            }
        }
        checkWinCondition();
    }

    /**
     * @brief Coloca uma bandeira em uma célula oculta.
     * @param x A coordenada X da célula.
     * @param y A coordenada Y da célula.
     */
    void placeFlag(int x, int y) {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT || grid[y][x].state != HIDDEN) return;
        grid[y][x].state = FLAGGED;
    }

    /**
     * @brief Verifica se a condição de vitória foi atingida (todas as células não-minas reveladas).
     */
    void checkWinCondition() {
        int revealed_count = 0;
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if (grid[y][x].state == REVEALED) {
                    revealed_count++;
                }
            }
        }
        if (revealed_count == (GRID_WIDTH * GRID_HEIGHT) - NUM_MINES) {
            youWin = true;
        }
    }

    /**
     * @brief Revela uma célula oculta aleatória. Usado como último recurso pela IA em um impasse total.
     */
    void revealRandomHidden() {
        std::vector<CellCoord> hidden_cells;
        for (int y = 0; y < GRID_HEIGHT; ++y) for (int x = 0; x < GRID_WIDTH; ++x) {
            if (grid[y][x].state == HIDDEN) hidden_cells.emplace_back(x, y);
        }
        if (!hidden_cells.empty()) {
            int idx = std::uniform_int_distribution<>(0, hidden_cells.size() - 1)(gen_global);
            revealCell(hidden_cells[idx].first, hidden_cells[idx].second);
        }
    }
    
    /**
     * @brief Renderiza o estado atual do tabuleiro na tela usando SDL2.
     * @param renderer O renderizador SDL para desenhar.
     * @param font A fonte TTF para desenhar textos.
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

// ==========================================================
//               LÓGICA DO AGENTE HARDCODED
// ==========================================================

/**
 * @brief Aplica as regras lógicas básicas e determinísticas do Campo Minado.
 * @details Regra 1: Se o número de bandeiras ao redor de uma casa é igual ao número da casa, revela as vizinhas ocultas.
 * Regra 2: Se o número de casas ocultas é igual ao (número da casa - bandeiras), marca as ocultas como minas.
 * @param game O estado atual do jogo.
 * @return true se uma ação foi tomada, false caso contrário.
 */
bool applyBasicRules(Game& game) {
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (game.grid[y][x].state != REVEALED || game.grid[y][x].neighboringMines == 0) continue;

            int hiddenNeighbors = 0;
            int flaggedNeighbors = 0;
            std::vector<CellCoord> hiddenCells;

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                        if (game.grid[ny][nx].state == HIDDEN) {
                            hiddenNeighbors++;
                            hiddenCells.push_back({nx, ny});
                        } else if (game.grid[ny][nx].state == FLAGGED) {
                            flaggedNeighbors++;
                        }
                    }
                }
            }

            if (hiddenNeighbors > 0) {
                // Regra 1: Se flags == numero, revela os outros vizinhos ocultos.
                if (game.grid[y][x].neighboringMines == flaggedNeighbors) {
                    for (const auto& cell : hiddenCells) game.revealCell(cell.first, cell.second);
                    return true;
                }
                // Regra 2: Se casas ocultas == (numero - flags), marca os vizinhos ocultos.
                if ((game.grid[y][x].neighboringMines - flaggedNeighbors) == hiddenNeighbors) {
                    for (const auto& cell : hiddenCells) game.placeFlag(cell.first, cell.second);
                    return true;
                }
            }
        }
    }
    return false; // Nenhuma regra básica pôde ser aplicada.
}


// --- REGRA 3: Lógica de Força Bruta ---

/**
 * @brief Valida se uma configuração hipotética de minas na fronteira é consistente com os números revelados.
 * @return true se a configuração é válida, false caso contrário.
 */
bool isConfigValid(const Game& game, const std::vector<CellCoord>& frontier, const std::vector<bool>& mine_config, const std::vector<CellCoord>& number_cells) {
    for (const auto& nc : number_cells) {
        int x = nc.first;
        int y = nc.second;
        int mines_around = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                    // Soma minas que já estavam flagadas
                    if (game.grid[ny][nx].state == FLAGGED) {
                        mines_around++;
                    } else {
                        // Soma minas da configuração hipotética atual
                        for (size_t i = 0; i < frontier.size(); ++i) {
                            if (frontier[i].first == nx && frontier[i].second == ny && mine_config[i]) {
                                mines_around++;
                            }
                        }
                    }
                }
            }
        }
        // Se a contagem de minas ao redor não bate com o número da casa, a configuração é inválida.
        if (mines_around != game.grid[y][x].neighboringMines) return false;
    }
    return true; // Se todas as casas com número foram consistentes, a configuração é válida.
}

// Variável global para armazenar as soluções válidas encontradas pela recursão.
std::vector<std::vector<bool>> valid_solutions;

/**
 * @brief Função recursiva que gera todas as combinações de posicionamento de minas na fronteira e as testa.
 * @details Utiliza uma abordagem de backtracking para explorar as possibilidades.
 */
void findCombinations(const Game& game, const std::vector<CellCoord>& frontier, const std::vector<CellCoord>& number_cells,
                      std::vector<bool>& current_config, int start_index, int mines_to_place) {
    if (mines_to_place < 0) return; // Parada se colocamos mais minas do que devíamos.

    // Se chegamos ao fim da fronteira
    if (start_index == current_config.size()) {
        // Se usamos o número exato de minas restantes e a configuração é válida, a salvamos.
        if (mines_to_place == 0 && isConfigValid(game, frontier, current_config, number_cells)) {
            valid_solutions.push_back(current_config);
        }
        return;
    }

    // Ramo 1: Tenta colocar uma mina na posição atual
    current_config[start_index] = true;
    findCombinations(game, frontier, number_cells, current_config, start_index + 1, mines_to_place - 1);

    // Ramo 2: Tenta não colocar uma mina na posição atual (backtrack)
    current_config[start_index] = false;
    findCombinations(game, frontier, number_cells, current_config, start_index + 1, mines_to_place);
}

/**
 * @brief Tenta resolver o jogo usando análise de força bruta na fronteira de casas ocultas.
 * @details Esta é a Regra 3 da IA. Ela gera todas as configurações de minas válidas e, a partir delas,
 * determina se há jogadas 100% seguras. Se não houver, calcula a probabilidade de cada
 * casa na fronteira ser uma mina para realizar um "chute inteligente".
 * @param game O estado atual do jogo.
 * @param best_guess_cell Referência para armazenar a coordenada do melhor chute.
 * @param best_guess_prob Referência para armazenar a probabilidade do melhor chute.
 * @return Um enum MoveResult indicando o resultado da análise.
 */
MoveResult solveByBruteForce(Game& game, CellCoord& best_guess_cell, double& best_guess_prob) {
    // 1. Identifica a "fronteira" (casas ocultas vizinhas de números) e os números que a definem.
    std::vector<CellCoord> frontier;
    std::map<CellCoord, bool> is_frontier;
    std::vector<CellCoord> number_cells;
    std::map<CellCoord, bool> is_number_cell;
    int flags_placed = 0;

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (game.grid[y][x].state == FLAGGED) flags_placed++;
            if (game.grid[y][x].state == REVEALED && game.grid[y][x].neighboringMines > 0) {
                bool has_hidden_neighbor = false;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT && game.grid[ny][nx].state == HIDDEN) {
                        has_hidden_neighbor = true;
                        if (!is_frontier[{nx, ny}]) {
                            frontier.push_back({nx, ny});
                            is_frontier[{nx, ny}] = true;
                        }
                    }
                }
                if (has_hidden_neighbor && !is_number_cell[{x, y}]) {
                    number_cells.push_back({x, y});
                    is_number_cell[{x, y}] = true;
                }
            }
        }
    }
    
    // Limite de segurança para evitar "explosão combinatória" em fronteiras muito grandes.
    if (frontier.empty() || frontier.size() > 16) return MoveResult::FAILED;

    int mines_remaining = NUM_MINES - flags_placed;
    if (mines_remaining < 0) return MoveResult::FAILED;

    // 2. Encontra todas as configurações válidas.
    valid_solutions.clear();
    std::vector<bool> current_config(frontier.size(), false);
    findCombinations(game, frontier, number_cells, current_config, 0, mines_remaining);

    if (valid_solutions.empty()) return MoveResult::FAILED;

    // 3. Analisa as soluções para encontrar jogadas seguras.
    std::vector<int> mine_counts(frontier.size(), 0);
    for (const auto& sol : valid_solutions) {
        for (size_t i = 0; i < sol.size(); ++i) {
            if (sol[i]) mine_counts[i]++;
        }
    }
    
    for (size_t i = 0; i < frontier.size(); ++i) {
        // Se uma casa é mina em TODAS as soluções válidas, é uma mina garantida.
        if (mine_counts[i] == valid_solutions.size()) {
            game.placeFlag(frontier[i].first, frontier[i].second);
            return MoveResult::GUARANTEED_MOVE_FOUND;
        }
        // Se uma casa NUNCA é mina, é uma casa segura garantida.
        if (mine_counts[i] == 0) {
            game.revealCell(frontier[i].first, frontier[i].second);
            return MoveResult::GUARANTEED_MOVE_FOUND;
        }
    }
    
    // 4. Se não há jogadas seguras, calcula o melhor chute (menor probabilidade de ser mina).
    best_guess_prob = 1.0;
    for(size_t i = 0; i < frontier.size(); ++i) {
        double current_prob = static_cast<double>(mine_counts[i]) / valid_solutions.size();
        if (current_prob < best_guess_prob) {
            best_guess_prob = current_prob;
            best_guess_cell = frontier[i];
        }
    }
    return MoveResult::NO_GUARANTEED_MOVE;
}


/**
 * @brief Função principal do programa.
 */
int main(int argc, char* argv[]) {
    // Inicialização do SDL
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* window = SDL_CreateWindow("Campo Minado - Agente Hardcoded", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20);
    if (!font) { std::cerr << "Erro ao carregar fonte: " << TTF_GetError() << std::endl; return 1; }

    Game game;
    
    // Função lambda para iniciar um novo jogo de forma limpa.
    auto startNewGame = [&]() {
        game.initializeGrid();
        int startX = std::uniform_int_distribution<>(0, GRID_WIDTH - 1)(gen_global);
        int startY = std::uniform_int_distribution<>(0, GRID_HEIGHT - 1)(gen_global);
        game.startGameAt(startX, startY);
        std::cout << "\n--- NOVO JOGO INICIADO EM (" << startX << "," << startY << ") ---" << std::endl;
    };

    startNewGame(); // Inicia o primeiro jogo

    bool running = true;
    bool autoPlay = true;

    // Loop principal do jogo
    while (running) {
        // Processamento de eventos (input do usuário)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_r) { // Reinicia o jogo com 'R'
                    startNewGame();
                    autoPlay = true;
                }
            }
        }

        // Lógica de decisão da IA, executada a cada frame se o autoplay estiver ativo.
        if (autoPlay && !game.gameOver && !game.youWin) {
            // 1. Tenta aplicar as regras básicas e determinísticas.
            bool action_taken = applyBasicRules(game);
            
            // 2. Se as regras básicas falharem, aciona a análise complexa de força bruta.
            if (!action_taken) {
                std::cout << "Regras basicas nao encontraram jogada. Analisando a fronteira..." << std::endl;
                CellCoord best_guess_cell = {-1, -1};
                double best_guess_prob = 1.0;
                MoveResult result = solveByBruteForce(game, best_guess_cell, best_guess_prob);

                // 3. Processa o resultado da análise.
                if (result == MoveResult::GUARANTEED_MOVE_FOUND) {
                    std::cout << "Analise encontrou uma jogada 100% segura!" << std::endl;
                    action_taken = true;
                } else { // Caso de impasse.
                    std::cout << "IMPASSE: Nenhuma jogada 100% segura foi encontrada." << std::endl;
                    // 3a. Se a análise calculou um chute probabilístico.
                    if (result == MoveResult::NO_GUARANTEED_MOVE) {
                        std::cout << "Chutando a celula com menor probabilidade de ser uma bomba..." << std::endl;
                        std::cout << "Melhor chute: (" << best_guess_cell.first << ", " << best_guess_cell.second 
                                  << ") com P(Mina) = " << std::fixed << std::setprecision(2) << best_guess_prob * 100 << "%" << std::endl;
                        game.revealCell(best_guess_cell.first, best_guess_cell.second);
                    } else { // 3b. Se a análise falhou (result == FAILED), faz um chute totalmente aleatório.
                        std::cout << "Analise complexa falhou. Chutando uma celula aleatoria..." << std::endl;
                        game.revealRandomHidden();
                    }
                    action_taken = true;
                }
            }

            // Se nenhuma ação foi possível, o agente está preso e para de jogar.
            if (!action_taken && !game.gameOver && !game.youWin) {
                std::cout << "AGENTE PRESO: Nenhuma acao possivel. Reinicie (R) ou feche." << std::endl;
                autoPlay = false;
            }
        }

        // Seção de renderização
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        game.render(renderer, font);
        SDL_RenderPresent(renderer);

        SDL_Delay(150); // Pequena pausa para visualização
    }

    // Liberação de recursos
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}