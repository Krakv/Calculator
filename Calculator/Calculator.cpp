#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
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

static string ExprStr;
static char scanSymbol;
static int SI;
static string DIGIT = "0123456789";
static string LETTER = "abcdefghijklmnopqrstuvwxyz";
Tree* root;
List* letters = nullptr;



static void InsNode(Tree*& tree, char c) {
    tree = new Tree();
    tree->inf = c;
}

static void WriteExpr(Tree* tree) {
    if (tree != nullptr) {
        WriteExpr(tree->left);
        WriteExpr(tree->right);
        cout << tree->inf << endl;
    }
}

static void NextSymbol() {
    if (SI < ExprStr.size() - 1) {
        SI++;
        scanSymbol = ExprStr[SI];
    }
    else
        SI = -1;
}

static bool Contain(string syms, char sym) {
    for (int i = 0; i < syms.size(); i++) {
        if (sym == syms[i])
            return true;
    }
    return false;
}

static void Number(Tree* tree) {
    tree->inf = scanSymbol;
    NextSymbol();
    if (Contain(DIGIT, scanSymbol) && SI != -1) {
        tree->right = new Tree();
        Number(tree->right);
    }
}

static void ADDEND(Tree*& tree);

static void EXPRESSION(Tree*& tree) {
    Tree* O = nullptr;
    tree = nullptr;
    if (Contain(DIGIT, scanSymbol) || scanSymbol == '(') {
        ADDEND(O);
        while (((scanSymbol == '+') || (scanSymbol == '-')) && (SI != -1)) {
            InsNode(tree, scanSymbol);
            tree->left = O;
            NextSymbol();
            if (SI != -1) {
                ADDEND(tree->right);
            }
            O = tree;
        }
        if (tree == nullptr)
            tree = O;

    }
    else {
        cout << "Неверное начало выражения - " << SI << "\n";
    }
}

static void Factor(Tree*& tree);

static void ADDEND(Tree*& tree) {
    Tree* O = nullptr;
    if (Contain(DIGIT, scanSymbol) || scanSymbol == '(' || Contain(LETTER, scanSymbol)) {
        Factor(O);
        while (((scanSymbol == '*') || (scanSymbol == '/')) && (SI != -1)) {
            InsNode(tree, scanSymbol);
            tree->left = O;
            NextSymbol();
            if (SI != -1) {
                Factor(tree->right);
            }
            O = tree;
        }
        if (tree == nullptr)
            tree = O;

    }
    else {

        cout << "Неверное начало выражения - " << SI << "\n";
    }
}

static void Factor(Tree*& tree) {
    if (Contain(DIGIT, scanSymbol)) {
        tree = new Tree();
        Number(tree);
    }
    else if (Contain(LETTER, scanSymbol)) {
        InsNode(tree, scanSymbol);
        NextSymbol();
    }
    else {
        if (scanSymbol == '(') {
            NextSymbol();
            if (SI != -1)
                EXPRESSION(tree);
            if (SI != -1 && scanSymbol == ')')
                NextSymbol();
            else
                cout << "Нет завершающей скобки\n";
        }
        else {
            cout << "Неверное начало выражения - " << SI << "\n";
        }
    }
}

static int TakeNumber(Tree* tree, int num = 0) {
    if (tree->right != nullptr) {
        return TakeNumber(tree->right, ((int)tree->inf - 48 + num) * 10);
    }
    return num + (int)tree->inf - 48;
}

static int TakeLetter() {
    List* list = letters;
    while (list != nullptr && list->letter != scanSymbol) {
        list = list->next;
    }
    if (list != nullptr)
        return list->value;
    else {
        cout << "Введите значение переменной " << scanSymbol << ": ";
        int result;
        cin >> result;
        list = new List();
        list->letter = scanSymbol;
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
        return result;
    }
}

static int Calculate(Tree*& tree) {
    if (Contain(DIGIT, tree->inf))
        return TakeNumber(tree);
    else if (Contain(LETTER, tree->inf))
        return TakeLetter();
    else {
        int res1 = Calculate(tree->left);
        int res2 = Calculate(tree->right);
        if (tree->inf == '+')
            return res1 + res2;
        else if (tree->inf == '-' )
            return res1 - res2;
        else if (tree->inf == '*')
            return res1 * res2;
        else if(tree->inf == '/')
            return res1 / res2;
    }

}

int main()
{
    setlocale(LC_ALL, "Russian");

    root = nullptr;
    cout << "Введите выражение:\n";
    cin >> ExprStr;
    SI = 0;
    scanSymbol = ExprStr[SI];
    EXPRESSION(root);
    if (SI == -1){
        cout << "Выражение составлено правильно\n";
    }
    else {
        cout << "Присутствуют лишние символы\n";
    }
    if (root != nullptr) {
        cout << "tree:\n";
        WriteExpr(root);
        cout << endl << "Результат = " << Calculate(root) << endl;
    }
}
