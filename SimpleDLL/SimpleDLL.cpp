// SimpleDLL.cpp : Определяет экспортируемые функции для DLL.
//

#include "framework.h"
#include "SimpleDLL.h"


// Пример экспортированной переменной
SIMPLEDLL_API int nSimpleDLL=0;

// Пример экспортированной функции.
SIMPLEDLL_API int fnSimpleDLL(void)
{
    return 0;
}

// Конструктор для экспортированного класса.
CSimpleDLL::CSimpleDLL()
{
    return;
}
