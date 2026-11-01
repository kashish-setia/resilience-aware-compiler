#include "cfg/CFGBuilder.h"

namespace edge {

std::unique_ptr<CFG> CFGBuilder::build(const Program& program) {
    cfg_ = std::make_unique<CFG>();
    auto entry = cfg_->createBlock("Entry");
    cfg_->setEntryBlock(entry->id);
    currentBlock_ = entry;

    for (const auto& stmt : program.statements) {
        if (stmt) buildStatement(stmt.get());
    }

    auto exitBlock = cfg_->createBlock("Exit");
    cfg_->setExitBlock(exitBlock->id);

    if (currentBlock_) {
        cfg_->addEdge(currentBlock_->id, exitBlock->id, CFGEdgeType::FALLTHROUGH);
    }

    cfg_->finalizePredecessors();
    return std::move(cfg_);
}

void CFGBuilder::buildStatement(const Stmt* stmt) {
    if (auto* ifStmt = dynamic_cast<const IfStmt*>(stmt)) {
        buildIf(ifStmt);
    } else if (auto* whileStmt = dynamic_cast<const WhileStmt*>(stmt)) {
        buildWhile(whileStmt);
    } else if (auto* blockStmt = dynamic_cast<const BlockStmt*>(stmt)) {
        buildBlock(blockStmt);
    } else if (auto* chkStmt = dynamic_cast<const CheckpointStmt*>(stmt)) {
        // Create an explicit block for the checkpoint so it can be analyzed with precision
        if (!currentBlock_->statements.empty()) {
            auto chkBlock = cfg_->createBlock("Checkpoint_" + chkStmt->label);
            cfg_->addEdge(currentBlock_->id, chkBlock->id, CFGEdgeType::FALLTHROUGH);
            currentBlock_ = chkBlock;
        } else {
            currentBlock_->label = "Checkpoint_" + chkStmt->label;
        }
        currentBlock_->addStatement(chkStmt);

        auto postChk = cfg_->createBlock("PostCheckpoint");
        cfg_->addEdge(currentBlock_->id, postChk->id, CFGEdgeType::FALLTHROUGH);
        currentBlock_ = postChk;
    } else {
        currentBlock_->addStatement(stmt);
    }
}

void CFGBuilder::buildBlock(const BlockStmt* stmt) {
    for (const auto& s : stmt->statements) {
        if (s) buildStatement(s.get());
    }
}

void CFGBuilder::buildIf(const IfStmt* stmt) {
    auto condBlock = currentBlock_;
    condBlock->addStatement(stmt); // condition evaluation inside block

    auto thenBlock = cfg_->createBlock("IfThen");
    auto mergeBlock = cfg_->createBlock("IfMerge");

    cfg_->addEdge(condBlock->id, thenBlock->id, CFGEdgeType::TRUE_BRANCH);

    // Build then branch
    currentBlock_ = thenBlock;
    if (stmt->thenBranch) buildStatement(stmt->thenBranch.get());
    if (currentBlock_) {
        cfg_->addEdge(currentBlock_->id, mergeBlock->id, CFGEdgeType::FALLTHROUGH);
    }

    if (stmt->elseBranch) {
        auto elseBlock = cfg_->createBlock("IfElse");
        cfg_->addEdge(condBlock->id, elseBlock->id, CFGEdgeType::FALSE_BRANCH);

        currentBlock_ = elseBlock;
        buildStatement(stmt->elseBranch.get());
        if (currentBlock_) {
            cfg_->addEdge(currentBlock_->id, mergeBlock->id, CFGEdgeType::FALLTHROUGH);
        }
    } else {
        cfg_->addEdge(condBlock->id, mergeBlock->id, CFGEdgeType::FALSE_BRANCH);
    }

    currentBlock_ = mergeBlock;
}

void CFGBuilder::buildWhile(const WhileStmt* stmt) {
    auto preLoopBlock = currentBlock_;
    auto loopHeader = cfg_->createBlock("LoopHeader");
    auto loopBody = cfg_->createBlock("LoopBody");
    auto loopExit = cfg_->createBlock("LoopExit");

    cfg_->addEdge(preLoopBlock->id, loopHeader->id, CFGEdgeType::FALLTHROUGH);

    loopHeader->addStatement(stmt); // Condition evaluated in header
    cfg_->addEdge(loopHeader->id, loopBody->id, CFGEdgeType::TRUE_BRANCH);
    cfg_->addEdge(loopHeader->id, loopExit->id, CFGEdgeType::LOOP_EXIT);

    currentBlock_ = loopBody;
    if (stmt->body) buildStatement(stmt->body.get());
    if (currentBlock_) {
        cfg_->addEdge(currentBlock_->id, loopHeader->id, CFGEdgeType::LOOP_BACK);
    }

    currentBlock_ = loopExit;
}

} // namespace edge
