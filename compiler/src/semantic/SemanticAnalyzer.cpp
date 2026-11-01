#include "semantic/SemanticAnalyzer.h"

namespace edge {

SemanticAnalyzer::SemanticAnalyzer(DiagnosticEngine& diag)
    : diag_(diag) {}

bool SemanticAnalyzer::isNumeric(TypeKind type) const {
    return type == TypeKind::INT || type == TypeKind::FLOAT;
}

bool SemanticAnalyzer::checkTypeMatch(TypeKind expected, TypeKind actual, const SourceLocation& loc, const std::string& context) {
    if (expected == actual) return true;
    // Allow widening int -> float
    if (expected == TypeKind::FLOAT && actual == TypeKind::INT) return true;
    if (expected == TypeKind::UNKNOWN || actual == TypeKind::UNKNOWN) return false;

    diag_.error("Type mismatch in " + context + ": expected '" + typeToString(expected) + 
                "', got '" + typeToString(actual) + "'", loc);
    return false;
}

bool SemanticAnalyzer::analyze(Program& program) {
    program.accept(*this);
    return !diag_.hasErrors();
}

void SemanticAnalyzer::visit(Program& node) {
    for (const auto& stmt : node.statements) {
        if (stmt) stmt->accept(*this);
    }
}

void SemanticAnalyzer::visit(VarDeclStmt& node) {
    Symbol sym;
    sym.name = node.name;
    sym.type = node.type;
    sym.annotation = node.annotation;
    sym.location = node.location;
    sym.statementId = node.statementId;
    sym.sizeBytes = getTypeSize(node.type);

    if (node.initializer) {
        currentExprIsExternal_ = false;
        node.initializer->accept(*this);
        checkTypeMatch(node.type, currentExprType_, node.location, "variable initialization of '" + node.name + "'");
        sym.isAssigned = true;
        sym.isExternalSensor = currentExprIsExternal_;
    }

    if (!symbolTable_.define(sym)) {
        diag_.error("Duplicate declaration of variable '" + node.name + "' in current scope", node.location);
    }
}

void SemanticAnalyzer::visit(AssignStmt& node) {
    Symbol* sym = symbolTable_.lookup(node.name);
    if (!sym) {
        diag_.error("Assignment to undeclared variable '" + node.name + "'", node.location);
        return;
    }

    currentExprIsExternal_ = false;
    node.value->accept(*this);
    checkTypeMatch(sym->type, currentExprType_, node.location, "assignment to '" + node.name + "'");
    sym->isAssigned = true;
    if (currentExprIsExternal_) {
        sym->isExternalSensor = true;
    }
}

void SemanticAnalyzer::visit(BlockStmt& node) {
    symbolTable_.enterScope();
    for (const auto& stmt : node.statements) {
        if (stmt) stmt->accept(*this);
    }
    symbolTable_.exitScope();
}

void SemanticAnalyzer::visit(IfStmt& node) {
    node.condition->accept(*this);
    if (currentExprType_ != TypeKind::BOOL && currentExprType_ != TypeKind::UNKNOWN) {
        diag_.error("If condition must evaluate to 'bool', got '" + typeToString(currentExprType_) + "'", node.condition->location);
    }

    if (node.thenBranch) node.thenBranch->accept(*this);
    if (node.elseBranch) node.elseBranch->accept(*this);
}

void SemanticAnalyzer::visit(WhileStmt& node) {
    node.condition->accept(*this);
    if (currentExprType_ != TypeKind::BOOL && currentExprType_ != TypeKind::UNKNOWN) {
        diag_.error("While loop condition must evaluate to 'bool', got '" + typeToString(currentExprType_) + "'", node.condition->location);
    }

    if (node.body) node.body->accept(*this);
}

void SemanticAnalyzer::visit(PrintStmt& node) {
    node.expression->accept(*this);
}

void SemanticAnalyzer::visit(CheckpointStmt& /*node*/) {
    // Checkpoints are semantically valid anywhere inside execution flow
}

void SemanticAnalyzer::visit(FaultStmt& /*node*/) {
    // Faults are semantically valid execution simulation triggers
}

void SemanticAnalyzer::visit(LiteralExpr& node) {
    currentExprType_ = node.value.getType();
    node.evaluatedType = currentExprType_;
    currentExprIsExternal_ = false;
}

void SemanticAnalyzer::visit(VariableExpr& node) {
    Symbol* sym = symbolTable_.lookup(node.name);
    if (!sym) {
        diag_.error("Use of undeclared variable '" + node.name + "'", node.location);
        currentExprType_ = TypeKind::UNKNOWN;
        node.evaluatedType = TypeKind::UNKNOWN;
        return;
    }
    sym->isRead = true;
    currentExprType_ = sym->type;
    node.evaluatedType = sym->type;
    currentExprIsExternal_ = false;
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    node.left->accept(*this);
    TypeKind leftType = currentExprType_;

    node.right->accept(*this);
    TypeKind rightType = currentExprType_;

    currentExprIsExternal_ = false;

    switch (node.op) {
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::DIV:
        case BinaryOp::MOD:
            if (!isNumeric(leftType) || !isNumeric(rightType)) {
                diag_.error("Arithmetic operator '" + binaryOpToString(node.op) + "' requires numeric operands", node.location);
                currentExprType_ = TypeKind::UNKNOWN;
            } else if (leftType == TypeKind::FLOAT || rightType == TypeKind::FLOAT || node.op == BinaryOp::DIV) {
                // If division or float operand, promote to float
                currentExprType_ = (node.op == BinaryOp::MOD) ? TypeKind::INT : 
                                   ((leftType == TypeKind::FLOAT || rightType == TypeKind::FLOAT) ? TypeKind::FLOAT : TypeKind::INT);
            } else {
                currentExprType_ = TypeKind::INT;
            }
            break;

        case BinaryOp::LT:
        case BinaryOp::LTE:
        case BinaryOp::GT:
        case BinaryOp::GTE:
            if (!isNumeric(leftType) || !isNumeric(rightType)) {
                diag_.error("Relational operator '" + binaryOpToString(node.op) + "' requires numeric operands", node.location);
            }
            currentExprType_ = TypeKind::BOOL;
            break;

        case BinaryOp::EQ:
        case BinaryOp::NEQ:
            if (leftType != rightType && !(isNumeric(leftType) && isNumeric(rightType))) {
                diag_.error("Equality operator requires comparable types", node.location);
            }
            currentExprType_ = TypeKind::BOOL;
            break;

        case BinaryOp::AND:
        case BinaryOp::OR:
            if (leftType != TypeKind::BOOL || rightType != TypeKind::BOOL) {
                diag_.error("Logical operator '" + binaryOpToString(node.op) + "' requires boolean operands", node.location);
            }
            currentExprType_ = TypeKind::BOOL;
            break;
    }

    node.evaluatedType = currentExprType_;
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    node.operand->accept(*this);
    if (node.op == UnaryOp::NEG) {
        if (!isNumeric(currentExprType_)) {
            diag_.error("Unary '-' operator requires numeric operand", node.location);
        }
    } else if (node.op == UnaryOp::NOT) {
        if (currentExprType_ != TypeKind::BOOL) {
            diag_.error("Unary '!' operator requires boolean operand", node.location);
        }
        currentExprType_ = TypeKind::BOOL;
    }
    node.evaluatedType = currentExprType_;
}

void SemanticAnalyzer::visit(CallExpr& node) {
    if (node.callee == "read_sensor") {
        if (node.args.size() != 1) {
            diag_.error("read_sensor expects exactly 1 integer channel argument", node.location);
        } else {
            node.args[0]->accept(*this);
            if (currentExprType_ != TypeKind::INT) {
                diag_.error("Channel argument to read_sensor must be an integer", node.location);
            }
        }
        currentExprType_ = TypeKind::INT;
        node.evaluatedType = TypeKind::INT;
        currentExprIsExternal_ = true; // SENSOR READ IS NON-RECOMPUTABLE EXTERNAL SOURCE!
    } else if (node.callee == "abs") {
        if (node.args.size() != 1) {
            diag_.error("abs() expects exactly 1 numeric argument", node.location);
            currentExprType_ = TypeKind::UNKNOWN;
        } else {
            node.args[0]->accept(*this);
            if (!isNumeric(currentExprType_)) {
                diag_.error("abs() argument must be numeric", node.location);
            }
        }
        node.evaluatedType = currentExprType_;
    } else if (node.callee == "min" || node.callee == "max") {
        if (node.args.size() != 2) {
            diag_.error(node.callee + "() expects exactly 2 numeric arguments", node.location);
            currentExprType_ = TypeKind::UNKNOWN;
        } else {
            node.args[0]->accept(*this);
            TypeKind t1 = currentExprType_;
            node.args[1]->accept(*this);
            TypeKind t2 = currentExprType_;
            if (!isNumeric(t1) || !isNumeric(t2)) {
                diag_.error(node.callee + "() arguments must be numeric", node.location);
            }
            currentExprType_ = (t1 == TypeKind::FLOAT || t2 == TypeKind::FLOAT) ? TypeKind::FLOAT : TypeKind::INT;
        }
        node.evaluatedType = currentExprType_;
    } else {
        diag_.error("Call to unknown function '" + node.callee + "'", node.location);
        currentExprType_ = TypeKind::UNKNOWN;
        node.evaluatedType = TypeKind::UNKNOWN;
    }
}

} // namespace edge
