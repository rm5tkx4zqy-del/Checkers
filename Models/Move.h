#pragma once
#include <stdlib.h>

typedef int8_t POS_T;
// Один шаг шахматной шашки: исходная клетка, конечная клетка и возможная побитая фигура.

struct move_pos
{
    // Координаты начала перемещения.
    POS_T x, y;             // from
    // Координаты клетки назначения.
    POS_T x2, y2;           // to
    // Координаты побитой шашки; -1 означает обычный ход без взятия.
    POS_T xb = -1, yb = -1; // beaten

    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) : x(x), y(y), x2(x2), y2(y2)
    {
    }
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }
// Сравнение шагов по начальной и конечной клетке для обработки выбора игрока.

    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }
    // Проверка различия двух шагов через инверсию оператора ==.
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};
