#include <iostream>
#include <string>
#include <thread>
#include <bitset>
#include <intrin.h>
#include <exception>
#include "FileStream.h"
using namespace std;

// Дерево для хранения выражения
struct Tree {
    char inf;
    Tree* left = nullptr;
    Tree* right = nullptr;
};
// Список для хранения целочисленных значений переменных
struct List {
    char letter;
    int value;
    List* next = nullptr;
};
// Список для хранения выражений, считанных из файла
struct ListStr {
    string str = "";
    ListStr* next = nullptr;
};

static const string DIGIT = "0123456789"; // Массив символов чисел для проверки символа на принадлежность к этому массиву
static const string LETTER = "abcdefghijklmnopqrstuvwxyz"; // Массив символов переменных для проверки символа на принадлежность к этому массиву
static const string OPERATIONS = "+-*/^) "; // Массив допустимых символов в середине выражения
Tree* root; // Дерево выражения
HANDLE hSemLetter = CreateSemaphore(NULL, 1, 2, NULL); // Семафор для синхронизации доступа к списку значений переменных

HRESULT LoadAndCallSomeFunction(DWORD dwParam1, UINT* puParam2); // Функция явной загрузки библиотеки DLL
void PrintCodeError(int exitCode);

// Функция записи значения в дерево выражения
static void InsNode(Tree*& tree, char c) {
    tree = new Tree();
    tree->inf = c;
}

// Функция взятия следующего символа (следующего индекса)
static int NextSymbol(int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения программы
    if (*SI < ExprStr.size() - 1) {
        *SI = *SI + 1;
    }
    else
        exitCode |= 0b1; // Флажок окончания строки
    return exitCode;
}

// Функция для проверки символа на принадлежность к множеству символов в массиве
static bool Contain(string syms, char sym) {
    for (int i = 0; i < syms.size(); i++) {
        if (sym == syms[i])
            return true;
    }
    return false;
}

// Функция вывода дерева выражения
static void WriteExpr(Tree* tree) {
    if (tree != nullptr) {
        // Если текущий символ не является переменной или числом, вывести скобочку
        if (!Contain(DIGIT, tree->inf) && !Contain(LETTER, tree->inf))
            cout << "(";
        WriteExpr(tree->left);
        cout << tree->inf;
        WriteExpr(tree->right);
        // Если текущий символ не является переменной или числом, вывести скобочку
        if (!Contain(DIGIT, tree->inf) && !Contain(LETTER, tree->inf))
            cout << ")";
    }
}

// Функция считывания числа в дерево выражения
static int Number(Tree* tree, int * SI, string ExprStr) {
    int exitCode = 0;
    tree->inf = ExprStr[*SI];
    exitCode |= NextSymbol(SI, ExprStr);
    if (Contain(DIGIT, ExprStr[*SI]) && !(exitCode & 1)) {
        tree->right = new Tree();
        exitCode |= Number(tree->right, SI, ExprStr);
    }
    return exitCode;
}

static int ADDEND(Tree*& tree, int* SI, string ExprStr);

static int Factor(Tree*& tree, int* SI, string ExprStr); 

// Функция анализа выражения с основными операторами + и -
static int EXPRESSION(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции
    Tree* O = nullptr; 
    tree = nullptr;

    // Если текущее выражение начинается с числа, переменной или скобки, то
    if (Contain(DIGIT, ExprStr[*SI]) || Contain(LETTER, ExprStr[*SI]) || ExprStr[*SI] == '(') {

        // Получаем значение левой части выражения и забираем его код завершения
        exitCode |= ADDEND(O, SI, ExprStr);

        // Если следующий символ не допустим в середине выражения
        if (!Contain(OPERATIONS, ExprStr[*SI]) && !(exitCode & 1)) {
            cout << "Неверное продолжение выражения. Позиция: " << *SI + 1 << "\n";
            exitCode |= 0b1000; // добавляем код ошибки
        }

        // Пока части выражения соединяются операторами + и -
        while (((ExprStr[*SI] == '+') || (ExprStr[*SI] == '-')) && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]); // Вставляем символ оператора
            tree->left = O; // Левое поддерево заполняем левой частью выражения
            exitCode |= NextSymbol(SI, ExprStr); // берем следующий символ и забираем код завершения

            // Если это не конец выражения
            if (!(exitCode & 1)) 
                exitCode |= ADDEND(tree->right, SI, ExprStr); // Добавляем правую часть выражения
            else { // Иначе завершение неверное
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100; // Код ошибки неверного завершения выражения
            }

            // Теперь левым поддеревом становится все текущее выражение
            O = tree; 
        }
        // Если в выражении не было операторов + и -
        if (tree == nullptr)
            tree = O; // Выражение принимает значение своей левой части

    }
    else { // Иначе, выражение было неверно начато
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return exitCode | 0b10; // Код ошибки неверного начала выражения с операторами + и -
    }
    return exitCode;
}


// Функция анализа выражения с основными операторами *, /, ^
static int ADDEND(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции
    Tree* O = nullptr;

    // Если текущее выражение начинается с числа, переменной или скобки, то
    if (Contain(DIGIT, ExprStr[*SI]) || ExprStr[*SI] == '(' || Contain(LETTER, ExprStr[*SI])) {

        // Получаем значение левой части выражения и забираем его код завершения
        exitCode |= Factor(O, SI, ExprStr);

        // Пока части выражения соединяются операторами * и /
        while (((ExprStr[*SI] == '*') || (ExprStr[*SI] == '/') || (ExprStr[*SI] == '^')) && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]);
            tree->left = O;
            exitCode |= NextSymbol(SI, ExprStr);

            // Если это не конец выражения
            if (!(exitCode & 1)) {
                exitCode |= Factor(tree->right, SI, ExprStr); // Добавляем правую часть выражения
            }
            else { // Иначе завершение неверное
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100000; // Код ошибки неверного начала выражения с операторами * и /
            }

            // Теперь левым поддеревом становится все текущее выражение
            O = tree;
        }
        // Если в выражении не было операторов * и /
        if (tree == nullptr)
            tree = O; // Выражение принимает значение своей левой части
    }
    else { // Иначе, выражение было неверно начато
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return 0b10000; // Код ошибки неверного начала выражения с операторами * и /
    }
    return exitCode;
}

// Функция анализа  части выражения (число, переменная или другое выражение)
static int Factor(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции

    // Выбранный символ является цифрой
    if (Contain(DIGIT, ExprStr[*SI])) {
        tree = new Tree();
        // Считывание числа
        exitCode |= Number(tree, SI, ExprStr);
    }

    // Выбранный символ является переменной
    else if (Contain(LETTER, ExprStr[*SI])) {
        InsNode(tree, ExprStr[*SI]);
        exitCode |= NextSymbol(SI, ExprStr);
    }

    // Иначе
    else {
        // Если выбранный символ является скобкой
        if (ExprStr[*SI] == '(') {
            exitCode |= NextSymbol(SI, ExprStr);

            //Если не конец выражения
            if (!(exitCode & 1)) {
                // Значение текущей части выражения будет являться другим выражением
                exitCode |= EXPRESSION(tree, SI, ExprStr);
            }
            // Если не конец выражения, а текущий символ является закрывающей скобкой, то скобка пропускается
            if (!(exitCode & 1) && ExprStr[*SI] == ')')
                exitCode |= NextSymbol(SI, ExprStr);
            else { // Иначе
                cout << "Нет завершающей скобки\n";
                return exitCode | 0b1000000000; // Код ошибки отсутствия завершающей скобки
            }
        }
        else { // Иначе
            cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
            return exitCode | 0b100000000; // Неверное начало выражения, ожидалась скобка, число или переменная
        }
    }
    return exitCode;
}

// Функция извлечения числа из дерева выражения
static int TakeNumber(Tree* tree, int num = 0) {
    // Пока правое поддерево существует
    if (tree->right != nullptr) {
        // Собирать число из символов в дереве
        return TakeNumber(tree->right, ((int)tree->inf - 48 + num) * 10);
    }
    return num + (int)tree->inf - 48;
}

// Функция извлечения переменной
static int TakeLetter(char letter, List*& letters) {
    // Ожидание, пока эта функция используется
    WaitForSingleObject(hSemLetter, INFINITE);

    // Поиск переменной в списке
    List* list = letters;
    while (list != nullptr && list->letter != letter) {
        list = list->next;
    }

    // Если переменная найдена, выйти из функции
    if (list != nullptr) {
        ReleaseSemaphore(hSemLetter, 1, NULL);
        return list->value;
    }
    else { // Иначе вводится значение новой переменной
        // Запрос на ввод
        cout << endl << "Введите значение переменной " << letter << ": ";
        int result;
        cin >> result;
        while (cin.peek() != '\n' || cin.fail()) {
            cin.clear();
            cin.ignore(100000000000, '\n');
            cout << "Неправильный ввод целого числа\n";
            cout << endl << "Введите значение переменной " << letter << ": ";
            cin >> result;
        }

        // Создание элемента списка
        list = new List();
        list->letter = letter;
        list->value = result;

        // Если в списке переменных нет переменных
        if (letters == nullptr) {
            letters = list;
        }
        else { // Если в списке переменных есть значения
            // Поиск конца списка
            List* temp = letters;
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            //Добавление в конец
            temp->next = list;
        }
        // Завершение работы функции
        ReleaseSemaphore(hSemLetter, 1, NULL);
        return result;
    }
}

// Функция запроса ввода на вопрос "Повторить?"
bool AskForRepeat(string text = "Ошибка", bool hadPerems = true) {
    // Пользовательский текст
    cout << text << endl; 
    if (hadPerems) {
        cout << "Вы хотите исполнить эту функцию еще раз с другими входными данными (если имеются переменные)? Y(Да)/N(Нет)\n";
        string ans;
        cin >> ans;
        // В случае неверного ввода завершение работы
        if (cin.peek() == ' ' || cin.fail())
            return false;
        if (ans == "Y")
            return true;
    }
    return false;
}

// Фильтр для обработки исключений, связанных с арифметическим переполнением и делением на ноль
int ExceptionFilter(int code, int * exitCode, bool * hasError) {
    if (*hasError) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    if (code == EXCEPTION_INT_OVERFLOW) {
        // Если ответ на запрос положительный, то флаг входа в цикл принимает значение истина
        if (AskForRepeat("Похоже, что запись результата арифметической операции занимает слишком много места, чтобы записать его в регистр процессора. Продолжение выполнения программы приведет к неверному результату вычислений.")) {
            *hasError = true;
            return EXCEPTION_EXECUTE_HANDLER;
        }
        else
            return EXCEPTION_CONTINUE_EXECUTION;
    }
    else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        // Если ответ на запрос положительный, то флаг входа в цикл принимает значение истина
        if (AskForRepeat("Обнаружено деление на ноль. Измените значения переменных, или программа завершится аварийно."))
            *hasError = true;
        else
            *exitCode = -1;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    else
        return EXCEPTION_CONTINUE_SEARCH;
}

static void Calculate(Tree*& tree, List*& letters, int* res, int* exitCode, bool* hasError); // Функция для высчитывания значения дерева выражения

// Функция создания потоков, для параллельного вычисления результата частей выражения
void resultThread(Tree*& tree, List*& letters, int * res1, int * res2, int * exitCode, bool* hasError) {
    int exitCode1 = 0; 
    int exitCode2 = 0;
    thread thread1(Calculate, ref(tree->left), ref(letters), res1, &exitCode1, hasError);
    thread thread2(Calculate, ref(tree->right), ref(letters), res2, &exitCode2, hasError);
    thread1.join();
    thread2.join();
    *exitCode = min(exitCode1, exitCode2); // Минимальный код завершения программы => отрицательное значение свидетельствует о наличии ошибки
}

// Функция высчитывания результата работы операторов + - * /
void result(Tree*& tree, List*& letters, int* res, int* exitCode, bool * hasError) {
    __try {
        int res1 = 0; int res2 = 0;

        // Получаем результаты работ потоков
        resultThread(tree, letters, &res1, &res2, exitCode, hasError);

        // Сложение
        if (tree->inf == '+') {
            *res = res1 + res2;
            // Проверка на арифм. переполнение
            LoadAndCallSomeFunction(__readeflags(), 0);
        }
        // Вычитание
        else if (tree->inf == '-') {
            *res = res1 - res2;
            // Проверка на арифм. переполнение
            LoadAndCallSomeFunction(__readeflags(), 0);
        }
        // Умножение
        else if (tree->inf == '*') {
            *res = res1 * res2;
            // Проверка на арифм. переполнение
            LoadAndCallSomeFunction(__readeflags(), 0);
        }
        // Деление
        else if (tree->inf == '/') {
            *res = res1 / res2;
        }
        // Деление
        else if (tree->inf == '^') {
            *res = 1;
            for(int i = 0; i < res2; i++)
                *res *= res1;
        }
    }
    __except (ExceptionFilter(GetExceptionCode(), exitCode, hasError)) {
        letters = nullptr;
    }
}

// Функция высчитывания результата дерева выражения
static void Calculate(Tree*& tree, List*& letters, int * res, int * exitCode, bool * hasError) {
    // Если текущий символ цифра
    if (Contain(DIGIT, tree->inf)) {
        // Получаем число
        *res = TakeNumber(tree);
    }
    // Если текущий символ переменная
    else if (Contain(LETTER, tree->inf)) {
        // Получаем значение переменной
        *res = TakeLetter(tree->inf, letters);
    }
    // Иначе получаем значение текущего выражения
    else
        result(tree, letters, res, exitCode, hasError);
}

// Функция записи выражений из файла в список
static ListStr* GetNotes() {
    // Функция считывания из файла
    string fs = InputFormula("example.txt");

    ListStr* formulas = new ListStr();
    ListStr* list = formulas;
    ListStr* C = list;
    // Проход по символам считанной строки
    for (int i = 0; i < fs.length() && fs[i] != 'э'; i++) {
        // Пока не дойдем до разделителя добавляем символ в строку
        if (fs[i] != '\n') {
            if (fs[i] != '\r')
                list->str += fs[i];
        }
        else {
            // Подхогтовка к проверке выражения
            root = nullptr;
            int SI = 0;
            // Проверка выражения
            int exitCode = EXPRESSION(root, &SI, list->str);
            system("CLS");
            // Если выражение завершилось удачно
            if (exitCode == 1) {
                // записываем выражение в список
                list->next = new ListStr();
                C = list;
                list = list->next;
            }
            else // Иначе сбрасываем значение строки
                list->str = "";
        }
    }
    // Удаляем последний пустой элемент
    C->next = nullptr;
    if (formulas->str == "" && formulas->next == nullptr)
        formulas = nullptr;
    return formulas;
}

// Функция вывода записей из списка
static int PrintNotes(ListStr* formulas) {
    ListStr* list = formulas;
    int index = 1;
    cout << index++ << ". Свой пример" << endl;
    while (list != nullptr) {
        cout << index++ << ". " << list->str << endl;
        list = list->next;
    }
    cout << index << ". Добавить пример в файл" << endl;
    // Возвращаем номер последнего варианта
    return index;
}

// Функция запроса записи в файл
static void WriteNote() {
    string formula;
    cout << "Введите выражение, которое нужно сохранить: ";
    getline(cin, formula);
    // Функция записи в файл
    OutputFormula("example.txt", formula);
}

// переменная для хранения вызова функции
typedef HRESULT(CALLBACK* LPFNDLLFUNC1)(DWORD, UINT*);

// Функция явного вызова функции из библиотеки DLL
HRESULT LoadAndCallSomeFunction(DWORD dwParam1, UINT* puParam2)
{
    HINSTANCE hDLL;               // Обращение к DLL
    LPFNDLLFUNC1 lpfnDllFunc1;    // Указатель функции
    HRESULT hrReturnVal;

    // загрузка библиотеки
    hDLL = LoadLibrary(L"CheckFunc");
    if (NULL != hDLL)
    {
        lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress(hDLL, "CheckFunc");
        if (NULL != lpfnDllFunc1)
        {
            // вызов функции
            hrReturnVal = lpfnDllFunc1(dwParam1, puParam2);
        }
        else
        {
            // сообщение об ошибке
            hrReturnVal = ERROR_DELAY_LOAD_FAILED;
        }
        FreeLibrary(hDLL);
    }
    else
    {
        hrReturnVal = ERROR_DELAY_LOAD_FAILED;
    }
    return hrReturnVal;
}

// Вывод объяснения кода завершения
void PrintCodeError(int exitCode) {
    if (exitCode & 0b10) {
        cout << "Неверное начало выражения. Ожидалась открывающая скобка, число или переменная" << endl;
    }
    if (exitCode & 0b100) {
        cout << "Неверный конец выражения. Ожидалось число или переменная" << endl;
    }
    if (exitCode & 0b1000) {
        cout << "Неверное продолжение выражения. Ожидались операторы +-*/" << endl;
    }
    if (exitCode & 0b10000) {
        cout << "Неверное начало выражения. Ожидалась открывающая скобка, число или переменная" << endl;
    }
    if (exitCode & 0b100000) {
        cout << "Неверный конец выражения. Ожидалось число или переменная" << endl;
    }
    if (exitCode & 0b100000000) {
        cout << "Неверное начало выражения. Ожидалась скобка, число или переменная" << endl;
    }
    if (exitCode & 0b1000000000) {
        cout << "Неверный конец выражения, ожидалась закрывающая скобка" << endl;
    }
}

int main()
{
    setlocale(LC_ALL, "Russian");
    root = nullptr; // дерево выражения
    string ExprStr = "\0"; // переменная для выражения
    int SI = 0; // индекс выбранного символа
    int count; // количество вариантов в меню
    
    // Ввод выражений из файла
    ListStr* formulas = nullptr;
    formulas = GetNotes();
    count = PrintNotes(formulas);
    
    // Ввод номера варианта
    int optionNumber = 1;
    cout << "Введите номер:\n";
    cin >> optionNumber;
    // Неверный ввод
    if (cin.peek() != '\n'){
        cout << "Присутствуют ненужные символы.";
        system("PAUSE");
        return 1;
    }
    // Неверный ввод
    if (cin.fail()) {
        cout << "Ошибка чтения целого числа.";
        system("PAUSE");
        return 1;
    }
    // Пропуск символа (для работы getline)
    cin.ignore();
    if (optionNumber == 1) {
        cout << "Введите выражение:\n";
        getline(cin, ExprStr);
    }
    else {
        // Поиск варианта в списке
        for (int i = 2; i < optionNumber && formulas != nullptr; i++)
            formulas = formulas->next;
        if (formulas != nullptr)
            ExprStr = formulas->str;
    }
    // Если последний вариант
    if (count == optionNumber) {
        // Запись в файл
        WriteNote();
        system("PAUSE");
        return 1;
    }
    // Если выражение не определено
    if (ExprStr == "\0") {
        cout << "Неверное значение.";
        system("PAUSE");
        return 1;
    }

    // Проверка выражения
    int exitCode = EXPRESSION(root, &SI, ExprStr);

    // Если ошибок нет
    if (!(exitCode & 0xFFFE)) {
        if (exitCode & 1){
            cout << "Выражение составлено правильно\n";
        }
        else {
            cout << "Присутствуют лишние символы. Позиция: " << SI + 1 << endl;
        }
    }
    else // Иначе вывод объяснение кода завершения
        PrintCodeError(exitCode);

    // Если завершение выражения корректное
    if (root != nullptr && exitCode == 1) {
        // Вывод выражения
        cout << ExprStr << endl;

        // Подготовка переменных для высчитывания
        List* letters = nullptr;
        int result = 0; int exitCodeCalculate = 0;
        bool hasError = false;
        do {
            if (hasError) {
                cout << "Еще раз\n";
                hasError = false;
            }
            // Высчитывание результата дерева выражения
            Calculate(root, letters, &result, &exitCodeCalculate, &hasError);

        } while (hasError);

        // Если работа функции завершилась без ошибок
        if (!exitCodeCalculate) {

            // Вывод дерева выражения 
            cout << "tree:\n";
            WriteExpr(root);

            // Вывод результата
            cout << "=" << result << endl;
        }
    }
    system("PAUSE");
}
