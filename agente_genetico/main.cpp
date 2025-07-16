/**
 * @file main.cpp
 * @brief Implementação de um agente de IA que aprende a jogar Campo Minado usando um Algoritmo Genético.
 * @details Este programa evolui uma população de agentes (indivíduos), cada um com um conjunto de
 * regras (genes), para descobrir estratégias eficazes de jogo. O treinamento é feito
 * contra um conjunto pré-definido e fixo de tabuleiros para garantir uma avaliação justa.
 * A visualização (opcional) usa a biblioteca SDL2.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <random>
#include <fstream>
#include <thread>
#include <future>
#include <cmath>
#include <atomic>
#include <mutex>

// === Constantes do Jogo ===
// Define as propriedades do tabuleiro do Campo Minado.
const int CELL_SIZE = 20;
const int GRID_WIDTH = 10;
const int GRID_HEIGHT = 10;
const int NUM_MINES = 15;

// === Parâmetros do Algoritmo Genético ===
// Estes parâmetros controlam o processo de evolução.
// Parâmetros estruturais: NÃO ALTERE durante um treinamento para evitar corromper o save.
const int POPULATION_SIZE = 300;     // Número de indivíduos em cada geração.
const int NUM_RULES = 150;          // Número de regras no "cérebro" de cada indivíduo.

// Parâmetros de processo: Podem ser ajustados entre sessões de treinamento.
const double MUTATION_RATE = 0.02;    // Probabilidade de um gene sofrer uma mutação aleatória.
const double CROSSOVER_RATE = 0.8;   // Probabilidade de dois pais cruzarem seus genes.
const int TOURNAMENT_SIZE = 10;        // Número de indivíduos que competem na seleção de pais.

// === Parâmetros de Avaliação ===
const int FIXED_GAME_COUNT = 200;     // Número total de cenários de jogo a serem gerados no banco de testes.
const int GAMES_PER_GENERATION = 20;  // Número de cenários aleatórios do banco usados para avaliar cada geração.

// === Parâmetros de Desempenho e Visualização ===
const int NUM_THREADS = 8;            // Número de threads para processar a avaliação de fitness em paralelo.
const int NUM_INDIVIDUALS_TO_DISPLAY = 9; // Quantidade de melhores indivíduos a serem visualizados (0 para desativar).

// Gerador de números aleatórios global.
std::random_device rd_global;
std::mt19937 gen_global(rd_global());

// === Estruturas e Enums Fundamentais ===

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

/**
 * @struct Game
 * @brief Gerencia o estado de uma única partida de Campo Minado.
 */
struct Game {
    std::vector<std::vector<Cell>> grid;
    bool gameOver = false;
    bool youWin = false;

    Game() : grid(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH)) {}

    /**
     * @brief Inicializa o tabuleiro com um layout de minas pré-definido.
     * @param startX Coordenada X do ponto de partida seguro.
     * @param startY Coordenada Y do ponto de partida seguro.
     * @param mineGrid Matriz booleana que define a localização de todas as minas.
     */
    void initializeGridFixed(int startX, int startY, const std::vector<std::vector<bool>> &mineGrid) {
        gameOver = false;
        youWin = false;

        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                grid[y][x] = Cell();
                grid[y][x].isMine = mineGrid[y][x];
            }
        }

        // Calcula os números para cada célula com base nas minas vizinhas.
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
        // Revela a célula inicial segura.
        if (startX >= 0 && startY >= 0) {
            revealCell(startX, startY);
        }
    }

    /**
     * @brief Revela uma célula e, se for vazia, suas vizinhas recursivamente.
     */
    void revealCell(int x, int y) {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT || grid[y][x].state != HIDDEN) return;
        grid[y][x].state = REVEALED;
        if (grid[y][x].isMine) { gameOver = true; return; }
        if (grid[y][x].neighboringMines == 0) {
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) revealCell(x + dx, y + dy);
        }
        checkWinCondition();
    }

    /**
     * @brief Coloca uma bandeira em uma célula oculta.
     */
    void placeFlag(int x, int y) {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT || grid[y][x].state != HIDDEN) return;
        grid[y][x].state = FLAGGED;
    }

    /**
     * @brief Verifica se a condição de vitória foi atingida.
     */
    void checkWinCondition() {
        int revealedCount = 0;
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if (grid[y][x].state == REVEALED) {
                    revealedCount++;
                }
            }
        }
        if (revealedCount == (GRID_WIDTH * GRID_HEIGHT) - NUM_MINES) {
            youWin = true;
        }
    }

    /**
     * @brief Renderiza o estado atual do tabuleiro na tela (usado pela visualização).
     */
    void renderGrid(SDL_Renderer* renderer, TTF_Font* font, int offsetX, int offsetY) const {
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                SDL_Rect cellRect = {offsetX + x * CELL_SIZE, offsetY + y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
                if (grid[y][x].state == REVEALED) {
                    if (grid[y][x].isMine) {
                        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                    } else {
                        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                    }
                } else {
                    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                }
                SDL_RenderFillRect(renderer, &cellRect);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &cellRect);

                if (grid[y][x].state == REVEALED && grid[y][x].neighboringMines > 0 && !grid[y][x].isMine) {
                    SDL_Color textColor;
                    switch (grid[y][x].neighboringMines) {
                        case 1: textColor = {0, 0, 255}; break;
                        case 2: textColor = {0, 255, 0}; break;
                        case 3: textColor = {255, 0, 0}; break;
                        case 4: textColor = {0, 0, 128}; break;
                        case 5: textColor = {128, 0, 0}; break;
                        case 6: textColor = {0, 128, 128}; break;
                        case 7: textColor = {0, 0, 0}; break;
                        case 8: textColor = {128, 128, 128}; break;
                        default: textColor = {0, 0, 0}; break;
                    }
                    std::string text = std::to_string(grid[y][x].neighboringMines);
                    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text.c_str(), textColor);
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

                    int textWidth = textSurface->w;
                    int textHeight = textSurface->h;
                    SDL_FreeSurface(textSurface);

                    SDL_Rect textRect = {offsetX + x * CELL_SIZE + (CELL_SIZE - textWidth) / 2,
                                         offsetY + y * CELL_SIZE + (CELL_SIZE - textHeight) / 2,
                                         textWidth, textHeight};
                    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                    SDL_DestroyTexture(textTexture);
                } else if (grid[y][x].state == FLAGGED) {
                    SDL_Color textColor = {255, 0, 0};
                    SDL_Surface* textSurface = TTF_RenderText_Solid(font, "F", textColor);
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

                    int textWidth = textSurface->w;
                    int textHeight = textSurface->h;
                    SDL_FreeSurface(textSurface);

                    SDL_Rect textRect = {offsetX + x * CELL_SIZE + (CELL_SIZE - textWidth) / 2,
                                         offsetY + y * CELL_SIZE + (CELL_SIZE - textHeight) / 2,
                                         textWidth, textHeight};
                    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                    SDL_DestroyTexture(textTexture);
                }
            }
        }

        if (gameOver || youWin) {
            SDL_Color textColor = gameOver ? SDL_Color{255, 0, 0, 255} : SDL_Color{0, 255, 0, 255};
            std::string message = gameOver ? "Game Over!" : "You Win!";
            SDL_Surface* textSurface = TTF_RenderText_Solid(font, message.c_str(), textColor);
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

            int textWidth = textSurface->w;
            int textHeight = textSurface->h;
            SDL_FreeSurface(textSurface);

            int windowWidth = GRID_WIDTH * CELL_SIZE;
            int windowHeight = GRID_HEIGHT * CELL_SIZE;
            SDL_Rect textRect = {offsetX + (windowWidth - textWidth) / 2,
                                 offsetY + (windowHeight - textHeight) / 2,
                                 textWidth, textHeight};
            SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
            SDL_DestroyTexture(textTexture);
        }
    }
};

/**
 * @enum RuleAction
 * @brief Define as duas possíveis ações que uma regra pode executar.
 */
enum RuleAction {
    ACTION_REVEAL_HIDDEN,
    ACTION_PLACE_FLAG
};

/**
 * @struct Rule
 * @brief Representa um único "gene" ou "instinto" de uma IA.
 * @details Contém um conjunto de condições e uma ação a ser tomada se todas as condições forem satisfeitas.
 */
struct Rule {
    int numberCondition;      // Condição: número na casa revelada.
    int hiddenCondition;      // Condição: número de vizinhos ocultos.
    int flaggedCondition;     // Condição: número de vizinhos com bandeira.
    bool nearEdge;            // Condição: a casa está perto da borda?
    bool hasSpecificPattern;  // Condição: um padrão específico foi detectado? (Placeholder)
    int extendedScope;        // Condição: qual o tamanho da vizinhança a ser analisada?
    int priority;             // Prioridade da regra (regras de maior prioridade são testadas primeiro).
    RuleAction action;        // Ação a ser executada.
};

/**
 * @struct Individual
 * @brief Representa uma única IA na população.
 */
struct Individual {
    std::vector<Rule> rules;  // O "cérebro" ou "DNA" do indivíduo, composto por um conjunto de regras.
    double fitness = 0.0;     // Pontuação que mede o quão bem o indivíduo joga.
};

/**
 * @struct FixedGame
 * @brief Armazena a configuração de um cenário de teste pré-definido.
 */
struct FixedGame {
    int startX;
    int startY;
    std::vector<std::vector<bool>> mineGrid; // Mapa exato da localização das minas.
};

// Variável global para armazenar todos os cenários de teste carregados do arquivo.
std::vector<FixedGame> fixedGamesGlobal;


// === Funções do Algoritmo Genético ===

/**
 * @brief Cria um novo indivíduo com um conjunto de regras totalmente aleatórias.
 * @return Um objeto Individual.
 */
Individual createRandomIndividual() {
    Individual ind;
    ind.rules.resize(NUM_RULES);
    std::uniform_int_distribution<> dis_num(0, 8);
    std::uniform_int_distribution<> dis_hidden(0, 8);
    std::uniform_int_distribution<> dis_flagged(0, 8);
    std::uniform_int_distribution<> dis_bool(0, 1);
    std::uniform_int_distribution<> dis_scope(1, 2);
    std::uniform_int_distribution<> dis_priority(1, 10);

    for(int i = 0; i < NUM_RULES; i++) {
        ind.rules[i].numberCondition = dis_num(gen_global);
        ind.rules[i].hiddenCondition = dis_hidden(gen_global);
        ind.rules[i].flaggedCondition = dis_flagged(gen_global);
        ind.rules[i].nearEdge = dis_bool(gen_global);
        ind.rules[i].hasSpecificPattern = dis_bool(gen_global);
        ind.rules[i].extendedScope = dis_scope(gen_global);
        ind.rules[i].priority = dis_priority(gen_global);
        ind.rules[i].action = (dis_bool(gen_global) == 0) ? ACTION_REVEAL_HIDDEN : ACTION_PLACE_FLAG;
    }
    return ind;
}

/**
 * @brief Aplica o conjunto de regras de um indivíduo para jogar uma partida.
 * @param ind O indivíduo (IA) que está jogando.
 * @param game O estado atual do jogo.
 * @return true se alguma regra foi aplicada e uma ação foi tomada, false caso contrário.
 */
bool applyRules(Individual &ind, Game &game) {
    bool changed = false;
    std::vector<Rule> sortedRules = ind.rules;
    std::sort(sortedRules.begin(), sortedRules.end(), [](const Rule &a, const Rule &b) {
        return a.priority > b.priority;
    });

    for(auto &rule : sortedRules) {
        if (game.gameOver || game.youWin) return changed;

        for(int y = 0; y < GRID_HEIGHT; y++) {
            for(int x = 0; x < GRID_WIDTH; x++) {
                if (game.gameOver || game.youWin) return changed;
                if(game.grid[y][x].state == REVEALED && game.grid[y][x].neighboringMines == rule.numberCondition) {
                    int flagsCount = 0;
                    int hiddenCount = 0;
                    std::vector<std::pair<int, int>> hiddenCells;

                    for(int dy = -rule.extendedScope; dy <= rule.extendedScope; dy++) {
                        for(int dx = -rule.extendedScope; dx <= rule.extendedScope; dx++) {
                            int nx = x + dx;
                            int ny = y + dy;
                            if(nx <0 || nx >= GRID_WIDTH || ny <0 || ny >= GRID_HEIGHT) continue;
                            if(game.grid[ny][nx].state == FLAGGED) flagsCount++;
                            if(game.grid[ny][nx].state == HIDDEN) {
                                hiddenCount++;
                                hiddenCells.emplace_back(nx, ny);
                            }
                        }
                    }

                    if(hiddenCount != rule.hiddenCondition) continue;
                    if(flagsCount != rule.flaggedCondition) continue;
                    if(rule.nearEdge) {
                        bool isNearEdge = (x <=1 || x >= GRID_WIDTH-2 || y <=1 || y >= GRID_HEIGHT-2);
                        if(!isNearEdge) continue;
                    }
                    if(rule.hasSpecificPattern) {
                        bool patternDetected = false;
                        if(game.grid[y][x].neighboringMines == 2) {
                            patternDetected = true;
                        }
                        if(!patternDetected) continue;
                    }

                    if(rule.action == ACTION_REVEAL_HIDDEN) {
                        if(flagsCount == rule.numberCondition && hiddenCount > 0) {
                            for(auto &c : hiddenCells) {
                                if(!game.gameOver && !game.youWin) {
                                    game.revealCell(c.first, c.second);
                                    changed = true;
                                }
                            }
                        }
                    } else if(rule.action == ACTION_PLACE_FLAG) {
                        if((rule.numberCondition - 1) == flagsCount && hiddenCount == 1) {
                            if(!game.gameOver && !game.youWin) {
                                game.placeFlag(hiddenCells[0].first, hiddenCells[0].second);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }
    return changed;
}

/**
 * @brief Revela uma célula oculta aleatória. Usado como fallback quando a IA fica presa.
 */
void revealRandomCell(Game &game) {
    std::vector<std::pair<int, int>> hiddenCells;
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (game.grid[y][x].state == HIDDEN) {
                hiddenCells.emplace_back(x, y);
            }
        }
    }

    if (!hiddenCells.empty()) {
        std::uniform_int_distribution<> dis(0, hiddenCells.size() - 1);
        int idx = dis(gen_global);
        game.revealCell(hiddenCells[idx].first, hiddenCells[idx].second);
    }
}

/**
 * @brief Avalia o desempenho de um indivíduo em um conjunto de cenários de teste.
 * @details Calcula a pontuação de fitness com base em células seguras reveladas, bandeiras corretas,
 * penalidades por erros e um grande bônus por vitória.
 * @param ind O indivíduo a ser avaliado.
 * @param selectedGames O conjunto de cenários de teste para esta avaliação.
 * @param generationWins Contador atômico para o total de vitórias na geração.
 * @param generationGames Contador atômico para o total de jogos na geração.
 * @return O valor médio de fitness do indivíduo nos cenários.
 */
double evaluateIndividual(Individual &ind, const std::vector<FixedGame> &selectedGames, std::atomic<int> &generationWins, std::atomic<int> &generationGames) {
    double totalScore = 0.0;

    for(const auto &fg : selectedGames) {
        Game game;
        game.initializeGridFixed(fg.startX, fg.startY, fg.mineGrid);

        int actionsTaken = 0;
        bool changed = true;
        while(!game.gameOver && !game.youWin && changed) {
            changed = applyRules(ind, game);
            if(changed) {
                actionsTaken++;
            }
            if(!changed && !game.gameOver && !game.youWin) {
                revealRandomCell(game);
                actionsTaken++;
                changed = true;
            }
        }

        int safeRevealed = 0;
        int correctFlags = 0;
        int minesRevealed = 0;

        for(int y = 0; y < GRID_HEIGHT; y++) {
            for(int x = 0; x < GRID_WIDTH; x++) {
                if(!game.grid[y][x].isMine && game.grid[y][x].state == REVEALED) {
                    safeRevealed++;
                }
                if(game.grid[y][x].isMine && game.grid[y][x].state == FLAGGED) {
                    correctFlags++;
                }
                if(game.grid[y][x].isMine && game.grid[y][x].state == REVEALED) {
                    minesRevealed++;
                }
            }
        }

        double score = safeRevealed;
        score += correctFlags * 5.0;
        score -= minesRevealed * 50.0;
        score -= actionsTaken * 0.1;

        if(game.youWin) {
            score += 2000.0;
            generationWins++;
        }

        generationGames++;
        totalScore += score;
    }

    return totalScore / selectedGames.size();
}

/**
 * @brief Calcula a distância genética entre dois indivíduos. Usado no niching.
 */
double calculateGeneticDistance(const Individual &a, const Individual &b) {
    int distance = 0;
    for(int i = 0; i < NUM_RULES; i++) {
        if(a.rules[i].numberCondition != b.rules[i].numberCondition) distance++;
        if(a.rules[i].hiddenCondition != b.rules[i].hiddenCondition) distance++;
        if(a.rules[i].flaggedCondition != b.rules[i].flaggedCondition) distance++;
        if(a.rules[i].nearEdge != b.rules[i].nearEdge) distance++;
        if(a.rules[i].hasSpecificPattern != b.rules[i].hasSpecificPattern) distance++;
        if(a.rules[i].extendedScope != b.rules[i].extendedScope) distance++;
        if(a.rules[i].action != b.rules[i].action) distance++;
    }
    return static_cast<double>(distance) / (NUM_RULES * 7.0); // Normalizado
}

/**
 * @brief Seleciona um indivíduo da população para ser um "pai".
 * @details Usa o método de seleção por torneio. Um número de indivíduos (TOURNAMENT_SIZE)
 * é escolhido aleatoriamente, e o de maior fitness vence, sendo selecionado para reprodução.
 * Inclui uma forma simples de "niching" para favorecer a diversidade.
 * @param pop A população atual.
 * @return O indivíduo selecionado.
 */
Individual tournamentSelection(const std::vector<Individual> &pop) {
    std::uniform_int_distribution<> dis(0, static_cast<int>(pop.size()) - 1);
    Individual best = pop[dis(gen_global)];

    for(int i = 1; i < TOURNAMENT_SIZE; i++) {
        Individual competitor = pop[dis(gen_global)];
        if(competitor.fitness > best.fitness) {
            best = competitor;
        }
    }
    return best;
}

/**
 * @brief Realiza o cruzamento (reprodução) entre dois pais para gerar dois filhos.
 * @details Utiliza a técnica de crossover de múltiplos pontos, misturando os "genes" (regras)
 * dos pais em vários locais aleatórios para criar os filhos.
 */
void crossover(const Individual &p1, const Individual &p2, Individual &o1, Individual &o2) {
    std::uniform_int_distribution<> dis_points(1, NUM_RULES - 1);
    int numPoints = dis_points(gen_global);
    std::vector<int> points;
    while(points.size() < static_cast<size_t>(numPoints)) {
        int point = dis_points(gen_global);
        if(std::find(points.begin(), points.end(), point) == points.end()) {
            points.push_back(point);
        }
    }
    std::sort(points.begin(), points.end());

    o1.rules.resize(NUM_RULES);
    o2.rules.resize(NUM_RULES);
    int last = 0;
    bool toggle = false;
    for(auto point : points) {
        for(int i = last; i < point; i++) {
            if(toggle) {
                o1.rules[i] = p2.rules[i];
                o2.rules[i] = p1.rules[i];
            } else {
                o1.rules[i] = p1.rules[i];
                o2.rules[i] = p2.rules[i];
            }
        }
        toggle = !toggle;
        last = point;
    }
    for(int i = last; i < NUM_RULES; i++) {
        if(toggle) {
            o1.rules[i] = p2.rules[i];
            o2.rules[i] = p1.rules[i];
        } else {
            o1.rules[i] = p1.rules[i];
            o2.rules[i] = p2.rules[i];
        }
    }
}

/**
 * @brief Aplica mutações aleatórias a um indivíduo.
 * @details Percorre cada "gene" (valor dentro de uma regra) e, com uma pequena probabilidade
 * (MUTATION_RATE), o altera para um novo valor aleatório. É a fonte de inovação genética.
 */
void mutate(Individual &ind) {
    std::uniform_real_distribution<> dis_real(0.0, 1.0);
    std::uniform_int_distribution<> dis_num(0, 8);
    std::uniform_int_distribution<> dis_hidden(0, 8);
    std::uniform_int_distribution<> dis_flagged(0, 8);
    std::uniform_int_distribution<> dis_bool(0, 1);
    std::uniform_int_distribution<> dis_scope(1, 2);
    std::uniform_int_distribution<> dis_priority(1, 10);

    for(auto &rule : ind.rules) {
        if(dis_real(gen_global) < MUTATION_RATE) rule.numberCondition = dis_num(gen_global);
        if(dis_real(gen_global) < MUTATION_RATE) rule.hiddenCondition = dis_hidden(gen_global);
        if(dis_real(gen_global) < MUTATION_RATE) rule.flaggedCondition = dis_flagged(gen_global);
        if(dis_real(gen_global) < MUTATION_RATE) rule.nearEdge = dis_bool(gen_global);
        if(dis_real(gen_global) < MUTATION_RATE) rule.hasSpecificPattern = dis_bool(gen_global);
        if(dis_real(gen_global) < MUTATION_RATE) rule.extendedScope = dis_scope(gen_global);
        if(dis_real(gen_global) < MUTATION_RATE) rule.priority = dis_priority(gen_global);
        if(dis_real(gen_global) < MUTATION_RATE) rule.action = (dis_bool(gen_global) == 0) ? ACTION_REVEAL_HIDDEN : ACTION_PLACE_FLAG;
    }
}

// === Funções de Persistência e Arquivos ===

/**
 * @brief Salva o estado atual da população em um arquivo binário.
 * @details Salva cada membro das estruturas individualmente para garantir compatibilidade.
 * @param population O vetor de indivíduos a ser salvo.
 * @param filename O nome do arquivo (ex: "populacao_regras.dat").
 */
void savePopulation(const std::vector<Individual> &population, const std::string &filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if(!ofs) {
        std::cerr << "Erro ao salvar a populacao em " << filename << std::endl;
        return;
    }
    int popSize = static_cast<int>(population.size());
    ofs.write(reinterpret_cast<const char*>(&popSize), sizeof(popSize));
    for(const auto &ind : population) {
        int numRules = static_cast<int>(ind.rules.size());
        ofs.write(reinterpret_cast<const char*>(&numRules), sizeof(numRules));
        for(const auto &rule : ind.rules) {
            // Salva cada membro da regra individualmente
            ofs.write(reinterpret_cast<const char*>(&rule.numberCondition), sizeof(rule.numberCondition));
            ofs.write(reinterpret_cast<const char*>(&rule.hiddenCondition), sizeof(rule.hiddenCondition));
            ofs.write(reinterpret_cast<const char*>(&rule.flaggedCondition), sizeof(rule.flaggedCondition));
            ofs.write(reinterpret_cast<const char*>(&rule.nearEdge), sizeof(rule.nearEdge));
            ofs.write(reinterpret_cast<const char*>(&rule.hasSpecificPattern), sizeof(rule.hasSpecificPattern));
            ofs.write(reinterpret_cast<const char*>(&rule.extendedScope), sizeof(rule.extendedScope));
            ofs.write(reinterpret_cast<const char*>(&rule.priority), sizeof(rule.priority));
            ofs.write(reinterpret_cast<const char*>(&rule.action), sizeof(rule.action));
        }
        ofs.write(reinterpret_cast<const char*>(&ind.fitness), sizeof(ind.fitness));
    }
    ofs.close();
}

/**
 * @brief Carrega uma população previamente salva de um arquivo binário.
 * @details Lê cada membro das estruturas individualmente para garantir compatibilidade.
 * @return true se o carregamento for bem-sucedido, false caso contrário.
 */
bool loadPopulation(std::vector<Individual> &population, const std::string &filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if(!ifs) return false;

    int popSize;
    ifs.read(reinterpret_cast<char*>(&popSize), sizeof(popSize));
    if (ifs.fail() || popSize != POPULATION_SIZE) {
        if (!ifs.fail()) std::cerr << "AVISO: O tamanho da populacao no arquivo (" << popSize << ") e diferente do parametro atual (" << POPULATION_SIZE << "). Iniciando do zero." << std::endl;
        return false;
    }

    population.assign(popSize, Individual());
    for(int i = 0; i < popSize; i++) {
        int numRules;
        ifs.read(reinterpret_cast<char*>(&numRules), sizeof(numRules));
        if (ifs.fail() || numRules != NUM_RULES) {
            if(!ifs.fail()) std::cerr << "AVISO: O numero de regras no arquivo (" << numRules << ") e diferente do parametro atual (" << NUM_RULES << "). Iniciando do zero." << std::endl;
            return false;
        }
        population[i].rules.resize(numRules);
        for(int j = 0; j < numRules; j++) {
            // Lê cada membro da regra individualmente
            Rule& rule = population[i].rules[j];
            ifs.read(reinterpret_cast<char*>(&rule.numberCondition), sizeof(rule.numberCondition));
            ifs.read(reinterpret_cast<char*>(&rule.hiddenCondition), sizeof(rule.hiddenCondition));
            ifs.read(reinterpret_cast<char*>(&rule.flaggedCondition), sizeof(rule.flaggedCondition));
            ifs.read(reinterpret_cast<char*>(&rule.nearEdge), sizeof(rule.nearEdge));
            ifs.read(reinterpret_cast<char*>(&rule.hasSpecificPattern), sizeof(rule.hasSpecificPattern));
            ifs.read(reinterpret_cast<char*>(&rule.extendedScope), sizeof(rule.extendedScope));
            ifs.read(reinterpret_cast<char*>(&rule.priority), sizeof(rule.priority));
            ifs.read(reinterpret_cast<char*>(&rule.action), sizeof(rule.action));
        }
        ifs.read(reinterpret_cast<char*>(&population[i].fitness), sizeof(population[i].fitness));
        if(ifs.fail()){
             std::cerr << "AVISO: Falha ao ler dados do arquivo de populacao. O arquivo pode estar corrompido. Iniciando do zero." << std::endl;
             return false;
        }
    }
    ifs.close();
    return true;
}

/**
 * @brief Gera o banco de cenários de teste fixos e os salva em um arquivo.
 * @details Garante que cada cenário gerado tenha um ponto de partida seguro ("safe zone").
 * Esta função só precisa ser executada uma vez se o arquivo não existir.
 */
void generateFixedGames(const std::string &filename, int totalFixedGames) {
    std::ofstream ofs(filename, std::ios::binary);
    if(!ofs) {
        std::cerr << "Erro ao criar o arquivo de jogos fixos." << std::endl;
        return;
    }

    std::mt19937 gen_fixed(rd_global());
    std::uniform_int_distribution<> dis_pos(0, GRID_WIDTH - 1);

    for(int i = 0; i < totalFixedGames; i++) {
        FixedGame fg;
        fg.startX = dis_pos(gen_fixed);
        fg.startY = dis_pos(gen_fixed);
        fg.mineGrid.assign(GRID_HEIGHT, std::vector<bool>(GRID_WIDTH, false));

        int placedMines = 0;
        while (placedMines < NUM_MINES) {
            int x = dis_pos(gen_fixed);
            int y = dis_pos(gen_fixed);
            bool isInSafeZone = (x >= fg.startX - 1 && x <= fg.startX + 1 && y >= fg.startY - 1 && y <= fg.startY + 1);
            if (!fg.mineGrid[y][x] && !isInSafeZone) {
                fg.mineGrid[y][x] = true;
                placedMines++;
            }
        }

        ofs.write(reinterpret_cast<const char*>(&fg.startX), sizeof(fg.startX));
        ofs.write(reinterpret_cast<const char*>(&fg.startY), sizeof(fg.startY));
        for(int y = 0; y < GRID_HEIGHT; y++) {
            for(int x = 0; x < GRID_WIDTH; x++) {
                char mine = fg.mineGrid[y][x] ? 1 : 0;
                ofs.write(&mine, sizeof(char));
            }
        }
    }
    ofs.close();
}

/**
 * @brief Carrega o banco de cenários de teste do arquivo para a memória.
 */
bool loadFixedGames(std::vector<FixedGame> &fixedGames, const std::string &filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if(!ifs) return false;

    fixedGames.clear();
    while (ifs.peek() != EOF) {
        FixedGame fg;
        ifs.read(reinterpret_cast<char*>(&fg.startX), sizeof(fg.startX));
        ifs.read(reinterpret_cast<char*>(&fg.startY), sizeof(fg.startY));
        if(ifs.fail()) break;

        fg.mineGrid.assign(GRID_HEIGHT, std::vector<bool>(GRID_WIDTH, false));
        for(int y = 0; y < GRID_HEIGHT; y++) {
            for(int x = 0; x < GRID_WIDTH; x++) {
                char mine;
                ifs.read(&mine, sizeof(char));
                fg.mineGrid[y][x] = (mine == 1);
            }
        }
        fixedGames.push_back(fg);
    }
    ifs.close();
    return true;
}

/**
 * @brief Seleciona um subconjunto aleatório de cenários de teste do banco global.
 * @details Este subconjunto será usado para avaliar a geração atual.
 * @param numGames O número de jogos a serem selecionados.
 * @return Um vetor de FixedGame.
 */
std::vector<FixedGame> selectFixedGames(int numGames) {
    std::vector<FixedGame> selectedGames;
    if (fixedGamesGlobal.empty()) return selectedGames;

    selectedGames.reserve(numGames);
    std::sample(fixedGamesGlobal.begin(), fixedGamesGlobal.end(), std::back_inserter(selectedGames),
                numGames, std::mt19937{std::random_device{}()});
    return selectedGames;
}

// === Funções de Visualização ===

/**
 * @brief Calcula o layout de grade para as janelas de visualização.
 */
void calculateGridLayout(int totalDisplays, int &rows, int &cols) {
    if (totalDisplays <= 0) {
        rows = 0;
        cols = 0;
        return;
    }
    cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(totalDisplays))));
    rows = static_cast<int>(std::ceil(static_cast<double>(totalDisplays) / cols));
}

/**
 * @brief Cria janelas SDL para visualizar os melhores indivíduos jogando em tempo real.
 * @details Esta função só é ativada se NUM_INDIVIDUALS_TO_DISPLAY for maior que 0.
 */
void visualizeTopN(const std::vector<Individual> &population, SDL_Window** windows, SDL_Renderer** renderers, TTF_Font* font, int numDisplays) {
    if (numDisplays <= 0) return;

    std::vector<Individual> sortedPop = population;
    std::sort(sortedPop.begin(), sortedPop.end(), [](const Individual &a, const Individual &b){
        return a.fitness > b.fitness;
    });

    int count = std::min(static_cast<int>(sortedPop.size()), numDisplays);
    std::vector<Game> games(count);
    for (int i = 0; i < count; i++) {
        games[i].initializeGridFixed(sortedPop[i].rules[0].numberCondition, sortedPop[i].rules[0].hiddenCondition, fixedGamesGlobal[i % fixedGamesGlobal.size()].mineGrid);
    }

    bool stillPlaying = true;
    while (stillPlaying) {
        stillPlaying = false;
        for (int i = 0; i < count; i++) {
            if (!games[i].gameOver && !games[i].youWin) {
                bool changed = applyRules(const_cast<Individual&>(sortedPop[i]), games[i]);
                if (!changed) {
                    revealRandomCell(games[i]);
                }
                stillPlaying = true; // Continua enquanto pelo menos um jogo estiver ativo.
            }
        }

        for (int i = 0; i < count; i++) {
            SDL_SetRenderDrawColor(renderers[i], 50, 50, 50, 255);
            SDL_RenderClear(renderers[i]);
            games[i].renderGrid(renderers[i], font, 0, 0);
            SDL_RenderPresent(renderers[i]);
        }
        SDL_Delay(100);
    }
}


/**
 * @brief Função principal do programa.
 * @details Gerencia o ciclo de vida do algoritmo genético: inicialização, avaliação,
 * seleção, reprodução e salvamento, repetindo por gerações.
 */
int main(int argc, char* argv[]) {
    std::vector<SDL_Window*> windowsVector;
    std::vector<SDL_Renderer*> renderersVector;
    TTF_Font* font = nullptr;

    // 1. Inicialização do SDL (se a visualização estiver ativa)
    if (NUM_INDIVIDUALS_TO_DISPLAY > 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << "Erro SDL: " << SDL_GetError() << std::endl; return 1; }
        if (TTF_Init() == -1) { std::cerr << "Erro TTF: " << TTF_GetError() << std::endl; SDL_Quit(); return 1; }
        
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
        if (!font) {
            std::cerr << "Erro ao carregar a fonte TTF: " << TTF_GetError() << std::endl;
            TTF_Quit(); SDL_Quit(); return 1;
        }

        int rows, cols;
        calculateGridLayout(NUM_INDIVIDUALS_TO_DISPLAY, rows, cols);
        
        SDL_DisplayMode DM;
        SDL_GetCurrentDisplayMode(0, &DM);
        int screenWidth = DM.w;
        int screenHeight = DM.h;

        int gridWidth = GRID_WIDTH * CELL_SIZE;
        int gridHeight = GRID_HEIGHT * CELL_SIZE;
        int spacing = 50;

        int totalWidth = cols * gridWidth + (cols - 1) * spacing;
        int totalHeight = rows * gridHeight + (rows - 1) * spacing;

        int startXCenter = (screenWidth - totalWidth) / 2;
        int startYCenter = (screenHeight - totalHeight) / 2;

        for (int i = 0; i < NUM_INDIVIDUALS_TO_DISPLAY; i++) {
            int row = i / cols;
            int col = i % cols;
            int startX = startXCenter + col * (gridWidth + spacing);
            int startY = startYCenter + row * (gridHeight + spacing);

            std::string title = "Individuo " + std::to_string(i + 1);
            SDL_Window* window = SDL_CreateWindow(title.c_str(),
                                      startX,
                                      startY,
                                      gridWidth,
                                      gridHeight,
                                      SDL_WINDOW_SHOWN);
            if (!window) {
                std::cerr << "Erro na criacao da janela " << i + 1 << ": " << SDL_GetError() << std::endl;
                return 1;
            }

            SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (!renderer) {
                std::cerr << "Erro ao criar renderer da janela " << i + 1 << ": " << SDL_GetError() << std::endl;
                return 1;
            }

            windowsVector.push_back(window);
            renderersVector.push_back(renderer);
        }
    }

    // 2. Carrega a população ou cria uma nova se não houver save.
    std::vector<Individual> population;
    if(!loadPopulation(population, "populacao_regras.dat")) {
        std::cout << "Nenhuma populacao salva encontrada. Criando uma nova populacao aleatoria..." << std::endl;
        population.reserve(POPULATION_SIZE);
        for (int i = 0; i < POPULATION_SIZE; i++) {
            population.push_back(createRandomIndividual());
        }
    } else {
        std::cout << "Populacao carregada de 'populacao_regras.dat'." << std::endl;
    }

    // 3. Gera ou carrega o banco de cenários de teste.
    std::string fixedGamesFile = "fixed_games.dat";
    std::ifstream infile(fixedGamesFile, std::ios::binary);
    if(!infile.good()) {
        std::cout << "Arquivo de jogos fixos nao encontrado. Gerando um novo..." << std::endl;
        generateFixedGames(fixedGamesFile, FIXED_GAME_COUNT);
    }
    infile.close();
    if(!loadFixedGames(fixedGamesGlobal, fixedGamesFile)) {
        std::cerr << "Erro fatal ao carregar os jogos fixos." << std::endl;
        return 1;
    }
    std::cout << fixedGamesGlobal.size() << " jogos fixos carregados." << std::endl;

    // 4. Início do Loop de Treinamento (Evolução)
    bool running = true;
    int generation = 1;
    while (running) {
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Iniciando Geracao: " << generation << std::endl;

        std::atomic<int> generationWins = 0;
        std::atomic<int> generationGames = 0;
        std::vector<FixedGame> currentFixedGames = selectFixedGames(GAMES_PER_GENERATION);

        // 4a. Avaliação de Fitness (em paralelo)
        std::vector<std::future<double>> futures;
        futures.reserve(POPULATION_SIZE);
        for (auto &ind : population) {
            futures.emplace_back(std::async(std::launch::async, [&]() -> double {
                // É seguro passar os contadores atômicos para a thread.
                return evaluateIndividual(ind, currentFixedGames, generationWins, generationGames);
            }));
        }
        for (size_t i = 0; i < population.size(); ++i) {
            population[i].fitness = futures[i].get();
        }

        // Ordena a população pelo fitness para encontrar o melhor.
        std::vector<Individual> sortedPop = population;
        std::sort(sortedPop.begin(), sortedPop.end(), [](const Individual &a, const Individual &b){
            return a.fitness > b.fitness;
        });

        // Exibe estatísticas da geração.
        std::cout << "Melhor Fitness da Geracao: " << sortedPop[0].fitness << std::endl;
        if (generationGames > 0) {
            double winRate = (static_cast<double>(generationWins) / generationGames) * 100.0;
            std::cout << "Taxa de Vitoria na Geracao: " << winRate << "% (" << generationWins << "/" << generationGames << ")" << std::endl;
        }

        // Visualiza os melhores indivíduos, se ativado.
        if (NUM_INDIVIDUALS_TO_DISPLAY > 0) {
            visualizeTopN(sortedPop, windowsVector.data(), renderersVector.data(), font, NUM_INDIVIDUALS_TO_DISPLAY);
        }

        // 4b. Criação da Próxima Geração (Seleção, Crossover, Mutação)
        std::vector<Individual> newPopulation;
        newPopulation.reserve(POPULATION_SIZE);
        
        // Elitismo: os 2 melhores indivíduos passam diretamente para a próxima geração.
        newPopulation.push_back(sortedPop[0]);
        if (sortedPop.size() > 1) newPopulation.push_back(sortedPop[1]);
        
        // Preenche o resto da nova população.
        std::uniform_real_distribution<> dis_prob(0.0, 1.0);
        while (static_cast<int>(newPopulation.size()) < POPULATION_SIZE) {
            Individual parent1 = tournamentSelection(population);
            Individual parent2 = tournamentSelection(population);
            Individual offspring1, offspring2;

            if (dis_prob(gen_global) < CROSSOVER_RATE) {
                crossover(parent1, parent2, offspring1, offspring2);
            } else {
                offspring1 = parent1;
                offspring2 = parent2;
            }

            mutate(offspring1);
            mutate(offspring2);

            newPopulation.push_back(offspring1);
            if (static_cast<int>(newPopulation.size()) < POPULATION_SIZE) {
                newPopulation.push_back(offspring2);
            }
        }
        
        population = newPopulation; // A nova geração substitui a antiga.

        // 4c. Salvamento e Finalização da Geração
        if(generation % 5 == 0) { // Salva o progresso a cada 5 gerações.
            std::cout << "Salvando progresso da populacao..." << std::endl;
            savePopulation(population, "populacao_regras.dat");
        }

        if (NUM_INDIVIDUALS_TO_DISPLAY > 0) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) running = false; }
        }

        generation++;
    }

    // 5. Limpeza dos recursos do SDL.
    if (NUM_INDIVIDUALS_TO_DISPLAY > 0) {
        TTF_CloseFont(font);
        for (auto& rend : renderersVector) SDL_DestroyRenderer(rend);
        for (auto& win : windowsVector) SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
    }

    return 0;
}