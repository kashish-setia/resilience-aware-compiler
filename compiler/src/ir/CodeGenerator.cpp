#include "ir/CodeGenerator.h"

namespace edge {

std::unique_ptr<BytecodeProgram> CodeGenerator::generate(const Program& program) {
    program_ = std::make_unique<BytecodeProgram>(program.name);
    const_cast<Program&>(program).accept(*this);
    program_->addInstruction(Instruction(OpCode::OP_HALT));
    return std::move(program_);
}

void CodeGenerator::emit(Instruction instr) {
    program_->addInstruction(std::move(instr));
}

void CodeGenerator::visit(Program& node) {
    for (const auto& s : node.statements) {
        if (s) s->accept(*this);
    }
}

void CodeGenerator::visit(VarDeclStmt& node) {
    if (node.initializer) {
        node.initializer->accept(*this);
    } else {
        // Default zero/false initialization
        if (node.type == TypeKind::INT) emit(Instruction(OpCode::OP_CONST, Value(0)));
        else if (node.type == TypeKind::FLOAT) emit(Instruction(OpCode::OP_CONST, Value(0.0)));
        else if (node.type == TypeKind::BOOL) emit(Instruction(OpCode::OP_CONST, Value(false)));
        else emit(Instruction(OpCode::OP_CONST, Value("")));
    }
    emit(Instruction(OpCode::OP_STORE_VAR, Value(), node.name, -1, node.statementId));
}

void CodeGenerator::visit(AssignStmt& node) {
    node.value->accept(*this);
    emit(Instruction(OpCode::OP_STORE_VAR, Value(), node.name, -1, node.statementId));
}

void CodeGenerator::visit(BlockStmt& node) {
    for (const auto& s : node.statements) {
        if (s) s->accept(*this);
    }
}

void CodeGenerator::visit(IfStmt& node) {
    node.condition->accept(*this);
    size_t jumpIfFalseAddr = program_->addInstruction(Instruction(OpCode::OP_JUMP_IF_FALSE, Value(), "", -1, node.statementId));

    if (node.thenBranch) node.thenBranch->accept(*this);

    if (node.elseBranch) {
        size_t jumpEndAddr = program_->addInstruction(Instruction(OpCode::OP_JUMP, Value(), "", -1, node.statementId));
        size_t elseStartAddr = program_->size();
        program_->patchJump(jumpIfFalseAddr, static_cast<int32_t>(elseStartAddr));

        node.elseBranch->accept(*this);
        size_t endAddr = program_->size();
        program_->patchJump(jumpEndAddr, static_cast<int32_t>(endAddr));
    } else {
        size_t endAddr = program_->size();
        program_->patchJump(jumpIfFalseAddr, static_cast<int32_t>(endAddr));
    }
}

void CodeGenerator::visit(WhileStmt& node) {
    size_t loopStartAddr = program_->size();
    node.condition->accept(*this);
    size_t jumpExitAddr = program_->addInstruction(Instruction(OpCode::OP_JUMP_IF_FALSE, Value(), "", -1, node.statementId));

    if (node.body) node.body->accept(*this);
    program_->addInstruction(Instruction(OpCode::OP_JUMP, Value(), "", static_cast<int32_t>(loopStartAddr), node.statementId));

    size_t loopExitAddr = program_->size();
    program_->patchJump(jumpExitAddr, static_cast<int32_t>(loopExitAddr));
}

void CodeGenerator::visit(PrintStmt& node) {
    node.expression->accept(*this);
    emit(Instruction(OpCode::OP_PRINT, Value(), "", -1, node.statementId));
}

void CodeGenerator::visit(CheckpointStmt& node) {
    emit(Instruction(OpCode::OP_CHECKPOINT, Value(static_cast<int64_t>(node.checkpointId)), node.label, -1, node.statementId));
}

void CodeGenerator::visit(FaultStmt& node) {
    emit(Instruction(OpCode::OP_FAULT, Value(node.reason), "", -1, node.statementId));
}

void CodeGenerator::visit(LiteralExpr& node) {
    emit(Instruction(OpCode::OP_CONST, node.value));
}

void CodeGenerator::visit(VariableExpr& node) {
    emit(Instruction(OpCode::OP_LOAD_VAR, Value(), node.name));
}

void CodeGenerator::visit(BinaryExpr& node) {
    node.left->accept(*this);
    node.right->accept(*this);

    switch (node.op) {
        case BinaryOp::ADD: emit(Instruction(OpCode::OP_ADD)); break;
        case BinaryOp::SUB: emit(Instruction(OpCode::OP_SUB)); break;
        case BinaryOp::MUL: emit(Instruction(OpCode::OP_MUL)); break;
        case BinaryOp::DIV: emit(Instruction(OpCode::OP_DIV)); break;
        case BinaryOp::MOD: emit(Instruction(OpCode::OP_MOD)); break;
        case BinaryOp::EQ: emit(Instruction(OpCode::OP_EQ)); break;
        case BinaryOp::NEQ: emit(Instruction(OpCode::OP_NEQ)); break;
        case BinaryOp::LT: emit(Instruction(OpCode::OP_LT)); break;
        case BinaryOp::LTE: emit(Instruction(OpCode::OP_LTE)); break;
        case BinaryOp::GT: emit(Instruction(OpCode::OP_GT)); break;
        case BinaryOp::GTE: emit(Instruction(OpCode::OP_GTE)); break;
        case BinaryOp::AND: emit(Instruction(OpCode::OP_AND)); break;
        case BinaryOp::OR: emit(Instruction(OpCode::OP_OR)); break;
    }
}

void CodeGenerator::visit(UnaryExpr& node) {
    node.operand->accept(*this);
    if (node.op == UnaryOp::NEG) {
        emit(Instruction(OpCode::OP_NEG));
    } else if (node.op == UnaryOp::NOT) {
        emit(Instruction(OpCode::OP_NOT));
    }
}

void CodeGenerator::visit(CallExpr& node) {
    if (node.callee == "read_sensor") {
        node.args[0]->accept(*this);
        emit(Instruction(OpCode::OP_READ_SENSOR));
    } else if (node.callee == "abs" || node.callee == "min" || node.callee == "max") {
        for (const auto& a : node.args) {
            a->accept(*this);
        }
        emit(Instruction(OpCode::OP_CALL_BUILTIN, Value(static_cast<int64_t>(node.args.size())), node.callee));
    }
}

} // namespace edge
