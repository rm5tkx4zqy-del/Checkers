#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    // Формируем все допустимые полные ходы и выбираем лучший при помощи минимакса.
    // Одно взятие с продолжениями считается ОДНИМ ходом, как требуют правила шашек.
    vector<move_pos> find_best_turns(const bool color) {
        const auto position = board->get_board();
        auto candidates = collect_complete_turns(position, color);
        if (candidates.empty()) return {};

        const size_t limit = static_cast<size_t>(std::max(0, Max_depth)) + 1;
        double best_value = -INF - 1.0;
        vector<move_pos> result;
        double alpha = -INF - 1.0;
        const double beta = INF + 1.0;

        for (const auto &candidate : candidates) {
            // После полного хода очередь переходит к сопернику.
            const double score = find_best_turns_rec(candidate.position, !color, color,
                                                     1, limit, alpha, beta);
            if (result.empty() || score > best_value) {
                best_value = score;
                result = candidate.path;
            }
            alpha = std::max(alpha, best_value);
        }
        return result;
    }

private:
    // Создаём НОВУЮ позицию после одного шага: взятие, превращение в дамку, перенос фигуры.
    // Исходный массив остаётся неизменным — это важно для перебора альтернатив.

    // Итог одного ПОЛНОГО хода, в том числе нескольких последовательных взятий.
    struct CompleteTurn {
        vector<vector<POS_T>> position;
        vector<move_pos> path;
    };

    // Раскрываем все допустимые ходы текущего игрока.
    // Правило обязательного взятия уже обеспечивается find_turns(color, position).
    vector<CompleteTurn> collect_complete_turns(const vector<vector<POS_T>> &position,
                                                bool active_color) {
        find_turns(active_color, position);
        const auto initial_moves = turns; // Копируем до запуска рекурсии.
        const bool must_capture = have_beats;
        vector<CompleteTurn> result;
        for (const auto &step : initial_moves) {
            vector<move_pos> path{step};
            const auto after = make_turn(position, step);
            if (must_capture) {
                // Одной шашкой может потребоваться совершить несколько взятий подряд.
                find_first_best_turn(after, step.x2, step.y2, path, result);
            } else {
                result.push_back({after, path});
            }
        }
        return result;
    }

    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }
// Оценка позиции для минимакса: число простых шашек и дамок;
// режим NumberAndPotential добавляет бонус за продвижение к дамке.

    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF;
        if (b + bq == 0)
            return 0;
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Достраиваем обязательную серию взятий одной шашкой.
    // До завершения всей серии право хода сопернику не передаётся.
    void find_first_best_turn(const vector<vector<POS_T>> &position, POS_T x, POS_T y,
                              vector<move_pos> &path, vector<CompleteTurn> &result) {
        find_turns(x, y, position);
        const auto continuations = turns; // Сохраняем: следующий рекурсивный вызов изменит turns.
        if (!have_beats) {
            result.push_back({position, path});
            return;
        }
        for (const auto &step : continuations) {
            path.push_back(step);
            find_first_best_turn(make_turn(position, step), step.x2, step.y2, path, result);
            path.pop_back(); // Откат ветки перебора.
        }
    }

    // Минимакс: наш бот максимизирует оценку, соперник минимизирует её.
    // alpha и beta задают границы отсечения невыгодных ветвей дерева поиска.
    double find_best_turns_rec(const vector<vector<POS_T>> &position, bool active_color,
                               bool bot_color, size_t depth, size_t limit,
                               double alpha, double beta) {
        // Глубина считается по законченным ходам, а не отдельным прыжкам при взятии.
        const auto candidates = collect_complete_turns(position, active_color);
        if (candidates.empty()) {
            // Игрок без допустимых ходов проигрывает независимо от числа шашек.
            return (active_color == bot_color) ? 0.0 : double(INF);
        }
        if (depth >= limit) return calc_score(position, bot_color);

        const bool maximizing = (active_color == bot_color);
        double best = maximizing ? -INF - 1.0 : INF + 1.0;
        for (const auto &candidate : candidates) {
            const double score = find_best_turns_rec(candidate.position, !active_color,
                                                     bot_color, depth + 1, limit, alpha, beta);
            if (maximizing) {
                best = std::max(best, score);
                alpha = std::max(alpha, best);
            } else {
                best = std::min(best, score);
                beta = std::min(beta, best);
            }
            // O0 - полный перебор, O1 - альфа-бета отсечение.
            if (optimization != "O0" && alpha >= beta) break;
        }
        return best;
    }

public:
    // Публичная перегрузка: вычислить допустимые ходы заданного цвета на текущей доске.
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }
// Публичная перегрузка: ходы конкретной шашки на текущей доске.

    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Приватная перегрузка: собрать все разрешённые ходы цвета в переданной позиции.
    // Если возможно взятие, простые ходы исключаются; варианты перемешиваются.
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }
// Приватная перегрузка: поиск взятий и обычных ходов одной шашки/дамки.
// Сначала проверяются взятия, затем простые диагональные переходы.

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    // Все доступные первые шаги, рассчитанные последним вызовом find_turns().
    vector<move_pos> turns;
    // Признак обязательного взятия в последнем наборе допустимых шагов.
    bool have_beats;
    // Глубина поиска без учёта дополнительного первого полухода; задаётся в Game::play().
    int Max_depth;

  private:
    // Генератор случайного порядка ходов для разнообразия игры.
    default_random_engine rand_eng;
    // Режим оценки позиции: NumberOnly или NumberAndPotential.
    string scoring_mode;
    // Режим оптимизации: O0 без отсечения, остальные режимы с alpha-beta.
    string optimization;
    // Хранит шаги оптимального пути в исходной реализации (до этапа 2).

    // Хранит индексы продолжений лучшего хода в исходной реализации (до этапа 2).

    // Ссылка на игровую доску (не владеем объектом).
    Board *board;
    // Ссылка на настройки игры (не владеем объектом).
    Config *config;
};
