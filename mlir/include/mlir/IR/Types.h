//===- Types.h - MLIR Type Classes ------------------------------*- C++ -*-===//
//
// Copyright 2019 The MLIR Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#ifndef MLIR_IR_TYPES_H
#define MLIR_IR_TYPES_H

#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"

namespace mlir {
  class MLIRContext;
  class PrimitiveType;

/// Integer identifier for all the concrete type kinds.
enum class TypeKind {
  // Integer.
  I1,
  I8,
  I16,
  I32,
  I64,

  // Target pointer sized integer.
  Int,

  // Floating point.
  BF16,
  F16,
  F32,
  F64,

  LAST_PRIMITIVE_TYPE = F64,

  // Derived types.
  Function,
  Vector,

  // TODO: Tensor / MemRef types.
};

/// Instances of the Type class are immutable, uniqued, immortal, and owned by
/// MLIRContext.  As such, they are passed around by raw non-const pointer.
///
class Type {
public:

  /// Return the classification for this type.
  TypeKind getKind() const {
    return kind;
  }

  /// Return true if this type is the specified kind.
  bool is(TypeKind k) const {
    return kind == k;
  }

  /// Return the LLVMContext in which this type was uniqued.
  MLIRContext *getContext() const { return context; }

  /// Print the current type.
  void print(raw_ostream &os) const;
  void dump() const;

  // Convenience factories.
  static PrimitiveType *getI1(MLIRContext *ctx);
  static PrimitiveType *getI8(MLIRContext *ctx);
  static PrimitiveType *getI16(MLIRContext *ctx);
  static PrimitiveType *getI32(MLIRContext *ctx);
  static PrimitiveType *getI64(MLIRContext *ctx);
  static PrimitiveType *getInt(MLIRContext *ctx);
  static PrimitiveType *getBF16(MLIRContext *ctx);
  static PrimitiveType *getF16(MLIRContext *ctx);
  static PrimitiveType *getF32(MLIRContext *ctx);
  static PrimitiveType *getF64(MLIRContext *ctx);

protected:
  explicit Type(TypeKind kind, MLIRContext *context)
    : context(context), kind(kind), subclassData(0) {
  }
  explicit Type(TypeKind kind, MLIRContext *context, unsigned subClassData)
    : Type(kind, context) {
    setSubclassData(subClassData);
  }

  ~Type() = default;

  unsigned getSubclassData() const { return subclassData; }

  void setSubclassData(unsigned val) {
    subclassData = val;
    // Ensure we don't have any accidental truncation.
    assert(getSubclassData() == val && "Subclass data too large for field");
  }

private:
  /// This refers to the MLIRContext in which this type was uniqued.
  MLIRContext *const context;

  /// Classification of the subclass, used for type checking.
  TypeKind kind : 8;

  // Space for subclasses to store data.
  unsigned subclassData : 24;
};

inline raw_ostream &operator<<(raw_ostream &os, const Type &type) {
  type.print(os);
  return os;
}

/// Primitive types are the atomic base of the type system, including integer
/// and floating point values.
class PrimitiveType : public Type {
public:
  static PrimitiveType *get(TypeKind kind, MLIRContext *context);

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const Type *T) {
    return T->getKind() <= TypeKind::LAST_PRIMITIVE_TYPE;
  }
private:
  PrimitiveType(TypeKind kind, MLIRContext *context);
};

inline PrimitiveType *Type::getI1(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::I1, ctx);
}
inline PrimitiveType *Type::getI8(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::I8, ctx);
}
inline PrimitiveType *Type::getI16(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::I16, ctx);
}
inline PrimitiveType *Type::getI32(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::I32, ctx);
}
inline PrimitiveType *Type::getI64(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::I64, ctx);
}
inline PrimitiveType *Type::getInt(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::Int, ctx);
}
inline PrimitiveType *Type::getBF16(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::BF16, ctx);
}
inline PrimitiveType *Type::getF16(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::F16, ctx);
}
inline PrimitiveType *Type::getF32(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::F32, ctx);
}
inline PrimitiveType *Type::getF64(MLIRContext *ctx) {
  return PrimitiveType::get(TypeKind::F64, ctx);
}


/// Function types map from a list of inputs to a list of results.
class FunctionType : public Type {
public:
  static FunctionType *get(ArrayRef<Type*> inputs, ArrayRef<Type*> results,
                           MLIRContext *context);

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const Type *T) {
    return T->getKind() == TypeKind::Function;
  }

  ArrayRef<Type*> getInputs() const {
    return ArrayRef<Type*>(inputsAndResults, getSubclassData());
  }

  ArrayRef<Type*> getResults() const {
    return ArrayRef<Type*>(inputsAndResults+getSubclassData(), numResults);
  }

private:
  unsigned numResults;
  Type *const *inputsAndResults;

  FunctionType(Type *const *inputsAndResults, unsigned numInputs,
               unsigned numResults, MLIRContext *context);
};


/// Vector types represent multi-dimensional SIMD vectors, and have fixed a
/// known constant shape with one or more dimension.
class VectorType : public Type {
public:
  static VectorType *get(ArrayRef<unsigned> shape, Type *elementType);

  ArrayRef<unsigned> getShape() const {
    return ArrayRef<unsigned>(shapeElements, getSubclassData());
  }

  PrimitiveType *getElementType() const {
    return elementType;
  }

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const Type *T) {
    return T->getKind() == TypeKind::Vector;
  }

private:
  const unsigned *shapeElements;
  PrimitiveType *elementType;

  VectorType(ArrayRef<unsigned> shape, PrimitiveType *elementType,
             MLIRContext *context);
};


} // end namespace mlir

#endif  // MLIR_IR_TYPES_H
