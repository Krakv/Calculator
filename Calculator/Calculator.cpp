#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <bitset>
#include <intrin.h>
#include <exception>
#include <cassert>
#include "FileStream.h"
#include "calculator_functions.h"
#include <float.h>
using namespace std;

// Дерево для хранения выражения
struct Tree {
    char inf = '\0';
    Tree* left = nullptr;
    Tree* right = nullptr;
};
// Список для хранения целочисленных значений переменных
struct List {
    string identifier;
    double value;
    List* next = nullptr;
};
// Список для хранения выражений, считанных из файла
struct ListStr {
    string str = "";
    ListStr* next = nullptr;
};

static FunctionRegistry registry;

static const string DIGIT = "0123456789"; // Массив символов чисел для проверки символа на принадлежность к этому массиву
static const string NUMBERSYMBOLS = "+-."; // Массив допустимых символов в начале числа
static const string LETTER = "abcdefghijklmnopqrstuvwxyz_"; // Массив символов переменных для проверки символа на принадлежность к этому массиву
static const string UPPERLETTER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // Массив символов переменных для проверки символа на принадлежность к этому массиву
static const string OPERATIONS = "+-*/^), "; // Массив допустимых символов в середине выражения
static const string COMPARERS = "><=!"; // Массив логических операторов сравнения
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
        if (tree->inf == '+' || tree->inf == '-' || tree->inf == '*' || tree->inf == '/' || tree->inf == '^' || tree->inf == ',' || tree->inf == '.')
            cout << "(";
        WriteExpr(tree->left);
        cout << tree->inf;
        WriteExpr(tree->right);
        // Если текущий символ не является переменной или числом, вывести скобочку
        if (tree->inf == '+' || tree->inf == '-' || tree->inf == '*' || tree->inf == '/' || tree->inf == '^' || tree->inf == ',' || tree->inf == '.')
            cout << ")";
    }
}

// Функци считывания беззнакового числа в дерево выражения
static int Uint(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0;
    InsNode(tree, ExprStr[*SI]);
    exitCode |= NextSymbol(SI, ExprStr);

    // Если следующий символ цифра, рекурсивно вызываем функцию для правого поддерева
    if (Contain(DIGIT, ExprStr[*SI]) && !(exitCode & 1)) {
        exitCode |= Uint(tree->right, SI, ExprStr);
    }
    return exitCode;
}

// Функци считывания целой части числа в дерево выражения
static int Int(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0;

    // Если число начинается со знака, добавляем знак в вершину, в левую часть 0
    if (ExprStr[*SI] == '+' || ExprStr[*SI] == '-') {
        InsNode(tree, ExprStr[*SI]);
        InsNode(tree->left, '0');
        exitCode |= NextSymbol(SI, ExprStr);
    }
    // Если следующий элемент цифра, вызываем чтение беззнакового числа и записываем число либо в правое поддерево (Если знак есть), либо в текущее дерево (Если знака нет)
    if (!(exitCode & 1) && Contain(DIGIT, ExprStr[*SI])) { 
        exitCode |= Uint(tree == nullptr ? tree : tree->right, SI, ExprStr);
    }
    // Иначе ошибка
    else {
        cout << "Неверное продолжение числа. Позиция: " << *SI + 1 << "\n";
        exitCode |= 0b10000000000;
    }

    return exitCode;
}

// Функция считывания числа в дерево выражения
static int Number(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0;
    // Любое число для калькулятора вещественное
    InsNode(tree, '.');
    // Если оно начинается со знака или цифры, значит есть целая часть
    if (!(exitCode & 1) && ExprStr[*SI] == '+' || ExprStr[*SI] == '-' || Contain(DIGIT, ExprStr[*SI])) {
        //Считываем целую часть в левое поддерево
        exitCode |= Int(tree->left, SI, ExprStr);
        // Если продолжается точкой, значит есть дробная часть
        if (!(exitCode & 1) && ExprStr[*SI] == '.') {
            exitCode |= NextSymbol(SI, ExprStr);
            // Если после точки цифры, считываем их в правое поддерево
            if (!(exitCode & 1) && Contain(DIGIT, ExprStr[*SI])) {
                exitCode |= Uint(tree->right, SI, ExprStr);
            }
            else {
                InsNode(tree->right, '0');
            }
        }
        else {
            InsNode(tree->right, '0');
        }
    }
    // Если число начинается со точки, значит целой части нет
    else if (!(exitCode & 1) && ExprStr[*SI] == '.') {
        InsNode(tree->left, '0');
        exitCode |= NextSymbol(SI, ExprStr);
        // Проверяем, идут ли после точки цифры, записываем в правое поддерево
        if (!(exitCode & 1) && Contain(DIGIT, ExprStr[*SI])) {
            exitCode |= Uint(tree->right, SI, ExprStr);
        }
        // Иначе число состоит из одной точки - ошибка
        else {
            cout << "Неверное продолжение числа. Позиция: " << *SI + 1 << "\n";
            exitCode |= 0b10000000000;
        }
    }
    return exitCode;
}

// Функция считывания переменной в дерево выражения
static int Identifier(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0;
    InsNode(tree, ExprStr[*SI]);
    exitCode |= NextSymbol(SI, ExprStr);

    // Если дальше идут цифры или буквы, сохраняем их в правое поддерево
    if (!(exitCode & 1) && (Contain(LETTER, ExprStr[*SI]) || Contain(DIGIT, ExprStr[*SI]))) {
        exitCode |= Identifier(tree->right, SI, ExprStr);
    }

    return exitCode;
}

static int ADDEND(Tree*& tree, int* SI, string ExprStr);

static int Factor(Tree*& tree, int* SI, string ExprStr);

static int COMPAREOR(Tree*& tree, int* SI, string ExprStr);

static int COMPAREAND(Tree*& tree, int* SI, string ExprStr);

static int Element(Tree*& tree, int* SI, string ExprStr);

static int Function(Tree*& tree, int* SI, string ExprStr);

// Функция анализа выражения с основными операторами + и -
static int EXPRESSION(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции
    Tree* O = nullptr;
    tree = nullptr;

    // Если текущее выражение начинается с числа, переменной или скобки, то
    if (Contain(DIGIT, ExprStr[*SI]) || Contain(LETTER, ExprStr[*SI]) || Contain(UPPERLETTER, ExprStr[*SI]) || ExprStr[*SI] == '(' || Contain(NUMBERSYMBOLS, ExprStr[*SI])) {

        // Получаем значение левой части выражения и забираем его код завершения
        exitCode |= ADDEND(O, SI, ExprStr);

        tree = O;

        // Если следующий символ не допустим в середине выражения
        if (!Contain(OPERATIONS, ExprStr[*SI]) && !(exitCode & 1)) {
            cout << "Неверное продолжение выражения. Позиция: " << *SI + 1 << "\n";
            exitCode |= 0b1000; // добавляем код ошибки
        }

        // Пока части выражения соединяются операторами + и -
        while (((ExprStr[*SI] == '+') || (ExprStr[*SI] == '-') ) && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]);
            tree->left = O; 
            exitCode |= NextSymbol(SI, ExprStr);

            // Если это не конец выражения
            if (!(exitCode & 1))
                exitCode |= ADDEND(tree->right, SI, ExprStr); // Добавляем правую часть выражения
            else { // Иначе завершение неверное
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100; // Код ошибки неверного завершения выражения
            }

            O = tree;
        }

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
    if (Contain(DIGIT, ExprStr[*SI]) || ExprStr[*SI] == '(' || Contain(LETTER, ExprStr[*SI]) || Contain(UPPERLETTER, ExprStr[*SI]) || Contain(NUMBERSYMBOLS, ExprStr[*SI])) {

        // Получаем значение левой части выражения и забираем его код завершения
        exitCode |= Factor(O, SI, ExprStr);

        // Пока части выражения соединяются операторами * / ^
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
                return exitCode | 0b100000; 
            }

            // Теперь левым поддеревом становится все текущее выражение
            O = tree;
        }
        
        if (tree == nullptr)
            tree = O; // Выражение принимает значение своей левой части
    }
    else { // Иначе, выражение было неверно начато
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return 0b10000; 
    }
    return exitCode;
}

// Функция анализа выражения с оператором |
static int Factor(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции
    Tree* O = nullptr;

    // Если текущее выражение начинается с числа, переменной или скобки, то
    if (Contain(DIGIT, ExprStr[*SI]) || ExprStr[*SI] == '(' || Contain(LETTER, ExprStr[*SI]) || Contain(UPPERLETTER, ExprStr[*SI]) || Contain(NUMBERSYMBOLS, ExprStr[*SI])) {

        // Получаем значение левой части выражения и забираем его код завершения
        exitCode |= COMPAREOR(O, SI, ExprStr);

        while ((ExprStr[*SI] == '|') && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]);
            tree->left = O;
            exitCode |= NextSymbol(SI, ExprStr);

            // Если это не конец выражения
            if (!(exitCode & 1)) {
                exitCode |= COMPAREOR(tree->right, SI, ExprStr); // Добавляем правую часть выражения
            }
            else { // Иначе завершение неверное
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100000; 
            }

            // Теперь левым поддеревом становится все текущее выражение
            O = tree;
        }
        
        if (tree == nullptr)
            tree = O; // Выражение принимает значение своей левой части
    }
    else { // Иначе, выражение было неверно начато
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return 0b10000; 
    }
    return exitCode;
}

// Функция анализа выражения с оператором &
static int COMPAREOR(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции
    Tree* O = nullptr;

    // Если текущее выражение начинается с числа, переменной или скобки, то
    if (Contain(DIGIT, ExprStr[*SI]) || ExprStr[*SI] == '(' || Contain(LETTER, ExprStr[*SI]) || Contain(UPPERLETTER, ExprStr[*SI]) || Contain(NUMBERSYMBOLS, ExprStr[*SI])) {

        // Получаем значение левой части выражения и забираем его код завершения
        exitCode |= COMPAREAND(O, SI, ExprStr);

        while ((ExprStr[*SI] == '&') && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]);
            tree->left = O;
            exitCode |= NextSymbol(SI, ExprStr);

            // Если это не конец выражения
            if (!(exitCode & 1)) {
                exitCode |= COMPAREAND(tree->right, SI, ExprStr); // Добавляем правую часть выражения
            }
            else { // Иначе завершение неверное
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100000; 
            }

            // Теперь левым поддеревом становится все текущее выражение
            O = tree;
        }
        
        if (tree == nullptr)
            tree = O; // Выражение принимает значение своей левой части
    }
    else { // Иначе, выражение было неверно начато
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return 0b10000; 
    }
    return exitCode;
}

// Функция анализа выражения с операторами > < = !
static int COMPAREAND(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции
    Tree* O = nullptr;

    // Если текущее выражение начинается с числа, переменной или скобки, то
    if (Contain(DIGIT, ExprStr[*SI]) || ExprStr[*SI] == '(' || Contain(LETTER, ExprStr[*SI]) || Contain(UPPERLETTER, ExprStr[*SI]) || Contain(NUMBERSYMBOLS, ExprStr[*SI])) {

        // Получаем значение левой части выражения и забираем его код завершения
        exitCode |= Element(O, SI, ExprStr);

        while ((Contain(COMPARERS, ExprStr[*SI])) && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]);
            tree->left = O;
            exitCode |= NextSymbol(SI, ExprStr);

            // Если это не конец выражения
            if (!(exitCode & 1)) {
                exitCode |= Element(tree->right, SI, ExprStr); // Добавляем правую часть выражения
            }
            else { // Иначе завершение неверное
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100000; 
            }

            // Теперь левым поддеревом становится все текущее выражение
            O = tree;
        }
        
        if (tree == nullptr)
            tree = O; // Выражение принимает значение своей левой части
    }
    else { // Иначе, выражение было неверно начато
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return 0b10000;
    }
    return exitCode;
}

// Функция анализа  части выражения (число, переменная или другое выражение)
static int Element(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции

    // Выбранный символ является цифрой
    if (Contain(DIGIT, ExprStr[*SI]) || Contain(NUMBERSYMBOLS, ExprStr[*SI])) {
        // Считывание числа
        exitCode |= Number(tree, SI, ExprStr);
    }

    // Выбранный символ является переменной
    else if (Contain(LETTER, ExprStr[*SI])) {
        exitCode |= Identifier(tree, SI, ExprStr);
    }

    else if (Contain(UPPERLETTER, ExprStr[*SI])) {
        exitCode |= Function(tree, SI, ExprStr);
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

// Функция анализа части выражения являющегося функцией
static int Function(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0; // Код завершения работы функции
    Tree* O = new Tree;
    tree = O;

    // Собираем название функции, один элемент содержит одну букву функции, название сохраняется в глубину правого поддерева
    vector<char> funcName;
    while (!(exitCode & 1) && Contain(UPPERLETTER, ExprStr[*SI])) {
        O->right = new Tree;
        O = O->right;
        O->inf = ExprStr[*SI];
        funcName.push_back(ExprStr[*SI]);
        exitCode |= NextSymbol(SI, ExprStr);
    }
    tree = tree->right;
    O->right = new Tree;
    O = O->right;

    string functionName(funcName.begin(), funcName.end());

    // Проверка на существование функции
    if (registry.hasFunction(functionName)) {
        // Если после названия функции идет открывающая скобка
        if (!(exitCode & 1) && ExprStr[*SI] == '(') {
            exitCode |= NextSymbol(SI, ExprStr);

            exitCode |= EXPRESSION(O->left, SI, ExprStr);

            int counter = 1;
            int argsCount = registry.getArgCount(functionName);

            //Если не конец выражения
            while (!(exitCode & 1) && ExprStr[*SI] == ',' && (++counter <= argsCount)) {
                exitCode |= NextSymbol(SI, ExprStr);
                O->inf = ',';
                O->right = new Tree;
                O = O->right;

                // Значение текущей части выражения будет являться другим выражением
                exitCode |= EXPRESSION(O->left, SI, ExprStr);
            }

            // Если количество аргументов не соответсвует количеству введенных параметров
            if (counter != argsCount) {
                cout << "Неверное количество аргументов. Требуется: " << argsCount << " Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b10000000000000000000000000; // Код ошибки неверного количества аргументов
            }

            // В конце функции должна быть закрывающая скобка
            if (!(exitCode & 1) && ExprStr[*SI] == ')') {
                exitCode |= NextSymbol(SI, ExprStr);
            }
            else {
                cout << "Неверный конец функции. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100; // Код ошибки неверного завершения выражения
            }
        }
        else {
            cout << "Неверное начало функции. Позиция: " << *SI + 1 << "\n";
            return exitCode | 0b1000000000000000000000000000;
        }
    }
    else {
        cout << "Неверное название функции. Позиция: " << *SI + 1 << "\n";
        return exitCode | 0b100000000000000000000000000;
    }
    return exitCode;
}

static void Calculate(Tree*& tree, List*& letters, double* res, int* exitCode, bool* hasError); // Функция для высчитывания значения дерева выражения

// Функция извлечения беззнакового числа из дерева выражения
static string TakeUint(Tree* tree, string num = "") {
    // Пока правое поддерево существует
    if (tree->right != nullptr) {
        // Собирать число из символов в дереве
        return TakeUint(tree->right, num + tree->inf);
    }
    return num + tree->inf;
}

// Функция извлечения целой части числа из дерева выражения
static string TakeInt(Tree* tree) {
    // Если со знаком, учитываем знак, если без, то не учитываем
    if (tree->inf == '-')
        return tree->inf + TakeUint(tree->right);
    else
        return TakeUint(tree);
}

// Функция извлечение числа из дерева выражения
static double TakeNumber(Tree* tree) {
    // Собираем целую часть числа
    string leftPart = TakeInt(tree->left);
    // Собираем дробную часть числа
    string rightPart = TakeUint(tree->right);
    string num = leftPart + ',' + rightPart;

    // string -> double
    double res = stod(num);

    return res;
}

// Функция извлечения результата работы функции из дерева выражения
static void TakeFunctionResult(Tree* tree, List*& identifiers, double* res, int* exitCode, bool* hasError) {
    // Собираем название функции по частям
    vector<char> funcName;
    while (isupper(tree->inf) && tree->inf != ',') {
        funcName.push_back(tree->inf);
        tree = tree->right;
    }

    string functionName(funcName.begin(), funcName.end());

    // Собираем значения параметров по частям
    vector<double> parameters;
    while (tree != nullptr) {
        double res1 = 0;
        // Каждый параметр - EXPRESSION, для каждого считаем результат
        Calculate(tree->left, identifiers, &res1, exitCode, hasError);
        parameters.push_back(res1);
        tree = tree->right;
    }

    // Получаем результат работы функции с параметрами
    *res = registry.callFunction(functionName, parameters);
}

// Функция извлечения переменной
static int TakeIdentifier(Tree* tree, List*& identifiers) {
    // Собираем переменную
    vector<char> identifier;
    while (tree != nullptr) {
        identifier.push_back(tree->inf);
        tree = tree->right;
    }

    string identifierStr(identifier.begin(), identifier.end());
    // Ожидание, пока эта функция используется
    WaitForSingleObject(hSemLetter, INFINITE);

    // Поиск переменной в списке
    List* list = identifiers;
    while (list != nullptr && list->identifier != identifierStr) {
        list = list->next;
    }

    // Если переменная найдена, выйти из функции
    if (list != nullptr) {
        ReleaseSemaphore(hSemLetter, 1, NULL);
        return list->value;
    }
    else { // Иначе вводится значение новой переменной
        // Запрос на ввод
        cout << endl << "Введите значение переменной " << identifierStr << ": ";
        int result;
        cin >> result;
        while (cin.peek() != '\n' || cin.fail()) {
            cin.clear();
            cin.ignore(100000000000, '\n');
            cout << "Неправильный ввод целого числа\n";
            cout << endl << "Введите значение переменной " << identifierStr << ": ";
            cin >> result;
        }

        // Создание элемента списка
        list = new List();
        list->identifier = identifierStr;
        list->value = result;

        // Если в списке переменных нет переменных
        if (identifiers == nullptr) {
            identifiers = list;
        }
        else { // Если в списке переменных есть значения
            // Поиск конца списка
            List* temp = identifiers;
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
int ExceptionFilter(int code, int* exitCode, bool* hasError) {
    if (*hasError) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    if (code == EXCEPTION_FLT_OVERFLOW) {
        // Если ответ на запрос положительный, то флаг входа в цикл принимает значение истина
        if (AskForRepeat("Похоже, что запись результата арифметической операции занимает слишком много места, чтобы записать его в регистр процессора. Продолжение выполнения программы приведет к неверному результату вычислений.")) {
            *hasError = true;
            return EXCEPTION_EXECUTE_HANDLER;
        }
        else
            return EXCEPTION_CONTINUE_EXECUTION;
    }
    else if (code == EXCEPTION_FLT_DIVIDE_BY_ZERO) {
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

// Функция создания потоков, для параллельного вычисления результата частей выражения
void resultThread(Tree*& tree, List*& identifiers, double* res1, double* res2, int* exitCode, bool* hasError) {
    int exitCode1 = 0;
    int exitCode2 = 0;
    thread thread1(Calculate, ref(tree->left), ref(identifiers), res1, &exitCode1, hasError);
    thread thread2(Calculate, ref(tree->right), ref(identifiers), res2, &exitCode2, hasError);
    thread1.join();
    thread2.join();
    *exitCode = min(exitCode1, exitCode2); // Минимальный код завершения программы => отрицательное значение свидетельствует о наличии ошибки
}

// Функция высчитывания результата работы операторов + - * / ^ & | > < = !
void result(Tree*& tree, List*& identifiers, double* res, int* exitCode, bool* hasError) {
    __try {
        double res1 = 0; double res2 = 0;
        // Получаем результаты работ потоков
        resultThread(tree, identifiers, &res1, &res2, exitCode, hasError);

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
        // Возведение в степень
        else if (tree->inf == '^') {
            *res = 1;
            for (int i = 0; i < res2; i++)
                *res *= res1;
        }
        else if (tree->inf == '>') {
            *res = res1 > res2;
        }
        else if (tree->inf == '<') {
            *res = res1 < res2;
        }
        else if (tree->inf == '=') {
            *res = res1 == res2;
        }
        else if (tree->inf == '!') {
            *res = res1 != res2;
        }
        else if (tree->inf == '&') {
            *res = res1 == true && res2 == true;
        }
        else if (tree->inf == '|') {
            *res = res1 == true || res2 == true;
        }
    }
    __except (ExceptionFilter(GetExceptionCode(), exitCode, hasError)) {
        identifiers = nullptr;
    }
}

// Функция высчитывания результата дерева выражения
static void Calculate(Tree*& tree, List*& identifiers, double* res, int* exitCode, bool* hasError) {
    // Если текущий символ цифра
    if (Contain(DIGIT, tree->inf) || tree->inf == '.') {
        // Получаем число
        *res = TakeNumber(tree);
    }
    // Если текущий символ переменная
    else if (Contain(LETTER, tree->inf)) {
        // Получаем значение переменной
        *res = TakeIdentifier(tree, identifiers);
    }
    else if (isupper(tree->inf)) {
        TakeFunctionResult(tree, identifiers, res, exitCode, hasError);
    }
    // Иначе получаем значение текущего выражения
    else
        result(tree, identifiers, res, exitCode, hasError);
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
            cout << list->str << endl;
            int exitCode = EXPRESSION(root, &SI, list->str);
            //system("CLS");
            // Если выражение завершилось удачно
            if (exitCode == 1) {
                cout << "Выражение составлено правильно" << endl;
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
    std::cout << "\033[2J\033[1;1H";
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
        cout << "Неверное продолжение выражения. Ожидались операторы +-*/^|&><=!" << endl;
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
    _controlfp_s(nullptr, 0, _EM_ZERODIVIDE); // Включение создания исключений с числами с плавающей точкой
    root = nullptr; // дерево выражения
    string ExprStr = "\0"; // переменная для выражения
    int SI = 0; // индекс выбранного символа
    int count; // количество вариантов в меню
    registerPluginFunctions(registry); // Регистрация функций, определенных в библиотеке
    
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
        double result = 0; int exitCodeCalculate = 0;
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
