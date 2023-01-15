#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/FileSystem.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Casting.h>
#include <memory>
static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static llvm::GlobalVariable *data;
static llvm::GlobalVariable *dataIndex;
llvm::Function *putFunc;
llvm::Function *getFunc;

using namespace llvm;
static void InitializeModule() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule =
      std::make_unique<llvm::Module>("My Brainfuck compiler", *TheContext);
  // Create a new builder for the module.
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

struct LoopBlock {
  llvm::BasicBlock *cond_block;
  llvm::BasicBlock *body_block;
  llvm::BasicBlock *end_block;
};

void forward() {
  llvm::Value *curIndex = Builder->CreateLoad(dataIndex);
  llvm::Value *incrementedIndex = Builder->CreateAdd(
      curIndex, llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 1)));
  Builder->CreateStore(incrementedIndex, dataIndex);
}

void backward() {
  llvm::Value *curIndex = Builder->CreateLoad(dataIndex);
  llvm::Value *incrementedIndex = Builder->CreateAdd(
      curIndex, llvm::ConstantInt::get(*TheContext, llvm::APInt(32, -1)));
  Builder->CreateStore(incrementedIndex, dataIndex);
}

void add() {
  // printf("%s", dataIndex->getType());
  // printf("hi\n");
  llvm::Value *curIndex = Builder->CreateLoad(dataIndex);
  llvm::Value *arrayAddr = Builder->CreateInBoundsGEP(data, curIndex);
  llvm::Value *currentVal =
      Builder->CreateLoad(llvm::Type::getInt32Ty(*TheContext), arrayAddr);
  llvm::Value *newVal = Builder->CreateAdd(
      currentVal, llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 1)));
  Builder->CreateStore(newVal, arrayAddr);
}

void minus() {
  llvm::Value *curIndex =
      Builder->CreateLoad(llvm::Type::getInt32Ty(*TheContext), dataIndex);
  llvm::Value *arrayAddr = Builder->CreateInBoundsGEP(data, curIndex);
  llvm::Value *currentVal =
      Builder->CreateLoad(llvm::Type::getInt32Ty(*TheContext), arrayAddr);
  llvm::Value *newVal = Builder->CreateAdd(
      currentVal, llvm::ConstantInt::get(*TheContext, llvm::APInt(32, -1)));
  Builder->CreateStore(newVal, arrayAddr);
}

void loopBegin(LoopBlock *loopBlockPtr, llvm::Function *mainfunc) {
  loopBlockPtr->cond_block =
      llvm::BasicBlock::Create(*TheContext, "loopCond", mainfunc);
  loopBlockPtr->body_block =
      llvm::BasicBlock::Create(*TheContext, "loopBody", mainfunc);
  loopBlockPtr->end_block =
      llvm::BasicBlock::Create(*TheContext, "loopEnd", mainfunc);

  Builder->CreateBr(loopBlockPtr->cond_block);
  Builder->SetInsertPoint(loopBlockPtr->cond_block);
  llvm::Value *curIndex = Builder->CreateLoad(dataIndex);
  llvm::Value *arrayAddr = Builder->CreateInBoundsGEP(data, curIndex);
  llvm::Value *currentVal =
      Builder->CreateLoad(llvm::Type::getInt32Ty(*TheContext), arrayAddr);
  Builder->CreateCondBr(Builder->CreateICmpNE(currentVal, Builder->getInt32(0)),
                        loopBlockPtr->body_block, loopBlockPtr->end_block);
  Builder->SetInsertPoint(loopBlockPtr->body_block);
}

void loopEnd(LoopBlock *loopBlockPtr) {
  Builder->CreateBr(loopBlockPtr->cond_block);
  Builder->SetInsertPoint(loopBlockPtr->end_block);
}

void putPtr() {
  llvm::FunctionType *ft = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(*TheContext), llvm::Type::getInt32Ty(*TheContext));
  llvm::Value *curIndex = Builder->CreateLoad(dataIndex);
  llvm::Value *arrayAddr = Builder->CreateInBoundsGEP(data, curIndex);
  llvm::Value *currentVal =
      Builder->CreateLoad(llvm::Type::getInt32Ty(*TheContext), arrayAddr);
  Builder->CreateCall(putFunc, std::vector<Value *>({currentVal}));
}

void getPtr() {
  llvm::FunctionType *ft =
      llvm::FunctionType::get(llvm::Type::getInt32Ty(*TheContext), false);
  llvm::Value *curIndex = Builder->CreateLoad(dataIndex);
  llvm::Value *arrayAddr = Builder->CreateInBoundsGEP(data, curIndex);
  llvm::Value *currentVal = Builder->CreateLoad(arrayAddr);
  llvm::Value *getValue =
      Builder->CreateTrunc(Builder->CreateCall(getFunc), Builder->getInt32Ty());
  Builder->CreateStore(getValue, currentVal);
}

int main() {
  InitializeModule();
  llvm::Type *elementType = llvm::Type::getInt32Ty(*TheContext);
  llvm::ArrayType *arrType = llvm::ArrayType::get(elementType, 100000);

  data = new llvm::GlobalVariable(
      *TheModule, arrType, false, llvm::GlobalVariable::CommonLinkage,
      llvm::ConstantAggregateZero::get(arrType), "global_array");

  dataIndex = new llvm::GlobalVariable(
      *TheModule, llvm::Type::getInt32Ty(*TheContext), false,
      llvm::GlobalValue::CommonLinkage,
      llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 0)), "dataIndex");

  llvm::FunctionType *putFuncType = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(*TheContext),
      std::vector<Type *>({llvm::Type::getInt32Ty(*TheContext)}), false);
  putFunc =
      llvm::Function::Create(putFuncType, llvm::GlobalValue::ExternalLinkage,
                             "putchar", TheModule.get());
  putFunc->setCallingConv(llvm::CallingConv::C);

  llvm::FunctionType *getFuncType = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(*TheContext),
      std::vector<Type *>({llvm::Type::getVoidTy(*TheContext)}), false);
  getFunc =
      llvm::Function::Create(getFuncType, llvm::GlobalValue::ExternalLinkage,
                             "getchar", TheModule.get());
  getFunc->setCallingConv(llvm::CallingConv::C);

  llvm::FunctionType *mainFuncType =
      llvm::FunctionType::get(llvm::Type::getInt32Ty(*TheContext), false);
  llvm::Function *mainFunc =
      llvm::Function::Create(mainFuncType, llvm::GlobalValue::ExternalLinkage,
                             "main", TheModule.get());

  LoopBlock loopStack[1000];
  LoopBlock *loopBlockPtr = loopStack;

  BasicBlock *entry_mian = BasicBlock::Create(*TheContext, "entry", mainFunc);
  // IRBuilder<> builder_main(entry_mian);
  Builder->SetInsertPoint(entry_mian);

  char curTok;
  while ((curTok = getchar()) != EOF) {
    printf("%c", curTok);
    fflush(stdout);
    switch (curTok) {
    case '>': // ++ptr;
      forward();
      break;
    case '<': // --ptr;
      backward();
      break;
    case '+': // ++*ptr;
      add();
      break;
    case '-': // --*ptr;
      minus();
      break;
    case '.': // putchar(*ptr);
      putPtr();
      break;
    case ',': // *ptr = getchar();
      getPtr();
      break;
    case '[': // while (*ptr) {
      loopBegin(loopBlockPtr++, mainFunc);
      break;
    case ']': //  }
      loopEnd(--loopBlockPtr);
      if (loopBlockPtr < loopStack) {
        fprintf(stderr, "unmatching ]\n");
        return 1;
      }
      break;
    default:
      break;
    }
  }

  Builder->CreateRet(Builder->getInt32(0));

  auto TargetTriple = llvm::sys::getDefaultTargetTriple();
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();
  std::string Error;
  auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

  // Print an error and exit if we couldn't find the requested target.
  // This generally occurs if we've forgotten to initialise the
  // TargetRegistry or we have a bogus target triple.
  if (!Target) {
    errs() << Error;
    return 1;
  }
  auto CPU = "generic";
  auto Features = "";

  TargetOptions opt;
  auto RM = Optional<Reloc::Model>();
  auto TargetMachine =
      Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
  TheModule->setDataLayout(TargetMachine->createDataLayout());
  TheModule->setTargetTriple(TargetTriple);
  auto Filename = "output.o";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message();
    return 1;
  }
  llvm::legacy::PassManager pass;
  auto FileType = CGFT_ObjectFile;

  if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
    errs() << "TargetMachine can't emit a file of this type";
    return 1;
  }

  pass.run(*TheModule);
  dest.flush();
  // TheModule->dump();

  // llvm::InitializeNativeTarget();
  // llvm::InitializeNativeTargetAsmPrinter();
  // llvm::InitializeNativeTargetAsmParser();
  // llvm::ExecutionEngine *ee = llvm::EngineBuilder(std::move(TheModule))
  //                                 .setEngineKind(llvm::EngineKind::JIT)
  //                                 .create();
  // void *mainAddr = ee->getPointerToFunction(mainFunc);

  // typedef int (*FuncType)();
  // FuncType mianFunc = (FuncType)mainAddr;
  // ee->finalizeObject();
  // std::cout << mianFunc() << std::endl;
  return 0;
}