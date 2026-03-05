// Приведенный ниже блок ifdef — это стандартный метод создания макросов, упрощающий процедуру
// экспорта из библиотек DLL. Все файлы данной DLL скомпилированы с использованием символа SIMPLEDLL_EXPORTS
// Символ, определенный в командной строке. Этот символ не должен быть определен в каком-либо проекте,
// использующем данную DLL. Благодаря этому любой другой проект, исходные файлы которого включают данный файл, видит
// функции SIMPLEDLL_API как импортированные из DLL, тогда как данная DLL видит символы,
// определяемые данным макросом, как экспортированные.
#ifdef SIMPLEDLL_EXPORTS
#define SIMPLEDLL_API __declspec(dllexport)
#else
#define SIMPLEDLL_API __declspec(dllimport)
#endif

// Этот класс экспортирован из библиотеки DLL
class SIMPLEDLL_API CSimpleDLL {
public:
	CSimpleDLL(void);
	// TODO: добавьте сюда свои методы.
};

extern SIMPLEDLL_API int nSimpleDLL;

SIMPLEDLL_API int fnSimpleDLL(void);
