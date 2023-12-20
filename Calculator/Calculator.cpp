#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <bitset>
#include <intrin.h>
#include <exception>
#include "FileStream.h"
using namespace std;

struct Tree {
    char inf;
    Tree* left = nullptr;
    Tree* right = nullptr;
};

struct List {
    char letter;
    int value;
    List* next = nullptr;
};

struct ListStr {
    string str = "";
    ListStr* next = nullptr;
};

static string DIGIT = "0123456789";
static string LETTER = "abcdefghijklmnopqrstuvwxyz";
static string OPERATIONS = "+-*/) ";
Tree* root;
HANDLE hSemLetter = CreateSemaphore(NULL, 1, 2, NULL);

HRESULT LoadAndCallSomeFunction(DWORD dwParam1, UINT* puParam2);

static void InsNode(Tree*& tree, char c) {
    tree = new Tree();
    tree->inf = c;
}

static int NextSymbol(int* SI, string ExprStr) {
    int exitCode = 0;
    if (*SI < ExprStr.size() - 1) {
        *SI = *SI + 1;
    }
    else
        exitCode |= 0b1;
    return exitCode;
}

static bool Contain(string syms, char sym) {
    for (int i = 0; i < syms.size(); i++) {
        if (sym == syms[i])
            return true;
    }
    return false;
}

static void WriteExpr(Tree* tree) {
    if (tree != nullptr) {
        if (!Contain(DIGIT, tree->inf) && !Contain(LETTER, tree->inf))
            cout << "(";
        WriteExpr(tree->left);
        cout << tree->inf;
        WriteExpr(tree->right);
        if (!Contain(DIGIT, tree->inf) && !Contain(LETTER, tree->inf))
            cout << ")";
    }
}

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

static int EXPRESSION(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0;
    Tree* O = nullptr;
    tree = nullptr;
    if (Contain(DIGIT, ExprStr[*SI]) || Contain(LETTER, ExprStr[*SI]) || ExprStr[*SI] == '(') {
        exitCode = exitCode | ADDEND(O, SI, ExprStr);
        if (!Contain(OPERATIONS, ExprStr[*SI]) && !(exitCode & 1)) {
            cout << "Неверное продолжение выражения. Позиция: " << *SI + 1 << "\n";
            exitCode |= 0b1000;
        }
        while (((ExprStr[*SI] == '+') || (ExprStr[*SI] == '-')) && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]);
            tree->left = O;
            exitCode |= NextSymbol(SI, ExprStr);

            if (!(exitCode & 1)) 
                exitCode |= ADDEND(tree->right, SI, ExprStr);
            else {
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100;
            }

            O = tree;
        }
        if (tree == nullptr)
            tree = O;

    }
    else {
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return exitCode | 0b10;
    }
    return exitCode;
}

static int ADDEND(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0;
    Tree* O = nullptr;
    if (Contain(DIGIT, ExprStr[*SI]) || ExprStr[*SI] == '(' || Contain(LETTER, ExprStr[*SI])) {
        exitCode |= Factor(O, SI, ExprStr);
        while (((ExprStr[*SI] == '*') || (ExprStr[*SI] == '/')) && !(exitCode & 1)) {
            InsNode(tree, ExprStr[*SI]);
            tree->left = O;
            exitCode |= NextSymbol(SI, ExprStr);
            if (!(exitCode & 1)) {
                exitCode |= Factor(tree->right, SI, ExprStr);
            }
            else {
                cout << "Неверный конец выражения. Позиция: " << *SI + 1 << "\n";
                return exitCode | 0b100000;
            }

            O = tree;
        }
        if (tree == nullptr)
            tree = O;
    }
    else {
        cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
        return 0b10000;
    }
    return exitCode;
}

static int Factor(Tree*& tree, int* SI, string ExprStr) {
    int exitCode = 0;
    if (Contain(DIGIT, ExprStr[*SI])) {
        tree = new Tree();
        exitCode |= Number(tree, SI, ExprStr);
    }
    else if (Contain(LETTER, ExprStr[*SI])) {
        InsNode(tree, ExprStr[*SI]);
        exitCode |= NextSymbol(SI, ExprStr);
    }
    else {
        if (ExprStr[*SI] == '(') {
            exitCode |= NextSymbol(SI, ExprStr);
            if (!(exitCode & 1)) {
                exitCode |= EXPRESSION(tree, SI, ExprStr);
            }
            if (!(exitCode & 1) && ExprStr[*SI] == ')')
                exitCode |= NextSymbol(SI, ExprStr);
            else {
                cout << "Нет завершающей скобки\n";
                return exitCode | 0b1000000000;
            }
        }
        else {
            cout << "Неверное начало выражения. Позиция: " << *SI + 1 << "\n";
            return exitCode | 0b100000000;
        }
    }
    return exitCode;
}

static int TakeNumber(Tree* tree, int num = 0) {
    if (tree->right != nullptr) {
        return TakeNumber(tree->right, ((int)tree->inf - 48 + num) * 10);
    }
    return num + (int)tree->inf - 48;
}

static int TakeLetter(char letter, List*& letters) {
    WaitForSingleObject(hSemLetter, INFINITE);
    List* list = letters;
    while (list != nullptr && list->letter != letter) {
        list = list->next;
    }
    if (list != nullptr) {
        ReleaseSemaphore(hSemLetter, 1, NULL);
        return list->value;
    }
    else {
        cout << endl << "Введите значение переменной " << letter << ": ";
        int result;
        cin >> result;
        list = new List();
        list->letter = letter;
        list->value = result;
        if (letters == nullptr) {
            letters = list;
        }
        else {
            List* temp = letters;
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            temp->next = list;
        }
        ReleaseSemaphore(hSemLetter, 1, NULL);
        return result;
    }
}

bool AskForRepeat(string text = "Ошибка", bool hadPerems = true) {
    cout << text << endl;
    if (hadPerems) {
        cout << "Вы хотите исполнить эту функцию еще раз с другими входными данными (если имеются переменные)? Y(Да)/N(Нет)\n";
        string ans;
        cin >> ans;
        if (ans == "Y")
            return true;
    }
    return false;
}

int ExceptionFilter(int code, bool * entryFlag) {
    if (code == EXCEPTION_INT_OVERFLOW) {
        if (AskForRepeat("Похоже, что запись результата арифметической операции занимает слишком много места, чтобы записать его в регистр процессора. Продолжение выполнения программы приведет к неверному результату вычислений.")) {
            *entryFlag = true;
            return EXCEPTION_EXECUTE_HANDLER;
        }
        else
            return EXCEPTION_CONTINUE_EXECUTION;
    }
    else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (AskForRepeat("Обнаружено деление на ноль. Измените значения переменных, или программа завершится аварийно."))
            *entryFlag = true;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    else
        return EXCEPTION_CONTINUE_SEARCH;
}

static void Calculate(Tree*& tree, List*& letters, int* res, int* exitCode);

void resultThread(Tree*& tree, List*& letters, int * res1, int * res2, int * exitCode) {
    int exitCode1 = 0;
    int exitCode2 = 0;
    thread thread1(Calculate, ref(tree->left), ref(letters), res1, &exitCode1);
    thread thread2(Calculate, ref(tree->right), ref(letters), res2, &exitCode2);
    thread1.join();
    thread2.join();
    *exitCode = min(exitCode1, exitCode2);
}

void result(Tree*& tree, List*& letters, int* res, int* exitCode) {
    bool entryFlag;
    do {
        entryFlag = false;
        __try {
            int res1 = 0; int res2 = 0;
            resultThread(tree, letters, &res1, &res2, exitCode);
            if (tree->inf == '+') {
                *res = res1 + res2;
                LoadAndCallSomeFunction(__readeflags(), 0);
            }
            else if (tree->inf == '-') {
                *res = res1 - res2;
                LoadAndCallSomeFunction(__readeflags(), 0);
            }
            else if (tree->inf == '*') {
                *res = res1 * res2;
                LoadAndCallSomeFunction(__readeflags(), 0);
            }
            else if (tree->inf == '/') {
                *res = res1 / res2;
                LoadAndCallSomeFunction(__readeflags(), 0);
            }
        }
        __except (ExceptionFilter(GetExceptionCode(), &entryFlag)) {
            letters = nullptr;
            if (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO && !exitCode) 
                *exitCode = -1;
        }
    } while (entryFlag);
}

static void Calculate(Tree*& tree, List*& letters, int * res, int * exitCode) {
    if (Contain(DIGIT, tree->inf)) {
        *res = TakeNumber(tree);
        exitCode = 0;
    }
    else if (Contain(LETTER, tree->inf)) {
        *res = TakeLetter(tree->inf, letters);
        exitCode = 0;
    }
    else
        result(tree, letters, res, exitCode);
}

static ListStr* GetNotes() {
    string fs = InputFormula("example.txt");
    ListStr* formulas = new ListStr();
    ListStr* list = formulas;
    ListStr* C = list;
    for (int i = 0; i < fs.length() && fs[i] != 'э'; i++) {
        if (fs[i] != '\n') {
            if (fs[i] != '\r')
                list->str += fs[i];
        }
        else {
            root = nullptr;
            int SI = 0;
            int exitCode = EXPRESSION(root, &SI, list->str);
            system("CLS");
            if (exitCode == 1) {
                list->next = new ListStr();
                C = list;
                list = list->next;
            }
            else
                list->str = "";
        }
    }
    C->next = nullptr;
    return formulas;
}

static int PrintNotes(ListStr* formulas) {
    ListStr* list = formulas;
    int index = 1;
    cout << index++ << ". Свой пример" << endl;
    while (list != nullptr) {
        cout << index++ << ". " << list->str << endl;
        list = list->next;
    }
    cout << index << ". Добавить пример в файл" << endl;
    return index;
}

static void WriteNote() {
    string formula;
    cout << "Введите выражение, которое нужно сохранить: ";
    cin >> formula;
    OutputFormula("example.txt", formula);
}

typedef HRESULT(CALLBACK* LPFNDLLFUNC1)(DWORD, UINT*);

HRESULT LoadAndCallSomeFunction(DWORD dwParam1, UINT* puParam2)
{
    HINSTANCE hDLL;               // Handle to DLL
    LPFNDLLFUNC1 lpfnDllFunc1;    // Function pointer
    HRESULT hrReturnVal;

    hDLL = LoadLibrary(L"CheckFunc");
    if (NULL != hDLL)
    {
        lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress(hDLL, "CheckFunc");
        if (NULL != lpfnDllFunc1)
        {
            // call the function
            hrReturnVal = lpfnDllFunc1(dwParam1, puParam2);
        }
        else
        {
            // report the error
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
    root = nullptr;
    string ExprStr = "\0";
    int SI = 0;
    int count;
    
    ListStr* formulas = nullptr;
    formulas = GetNotes();
    count = PrintNotes(formulas);
    
    int optionNumber = 1;
    cout << "Введите номер:\n";
    
    cin >> optionNumber;
    if (cin.peek() == ' '){
        cout << "Присутствует пробел.";
        return 1;
    }
    if (cin.fail()) {
        cout << "Ошибка чтения целого числа.";
        return 1;
    }
    cin.ignore();
    if (optionNumber == 1) {
        cout << "Введите выражение:\n";
        getline(cin, ExprStr);
    }
    else {
        for (int i = 2; i < optionNumber && formulas != nullptr; i++)
            formulas = formulas->next;
        if (formulas != nullptr)
            ExprStr = formulas->str;
    }
    if (count == optionNumber) {
        WriteNote();
        return 1;
    }
    if (ExprStr == "\0") {
        cout << "Неверное значение.";
        return 1;
    }

    int exitCode = EXPRESSION(root, &SI, ExprStr);
    if (!(exitCode & 0xFFFE)) {
        if (exitCode & 1){
            cout << "Выражение составлено правильно\n";
        }
        else {
            cout << "Присутствуют лишние символы. Позиция: " << SI + 1 << endl;
        }
    }
    else
        PrintCodeError(exitCode);

    if (root != nullptr && exitCode == 1) {
        cout << ExprStr << endl;
        List* letters = nullptr;
        int result; int exitCodeCalculate = 0;
        Calculate(root, letters, &result, &exitCodeCalculate);
        if (!exitCodeCalculate) {
            cout << "tree:\n";
            WriteExpr(root);
            cout << "=" << result << endl;
        }
    }
}
