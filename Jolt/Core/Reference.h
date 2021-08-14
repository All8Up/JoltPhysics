// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>

namespace JPH {

// Forward declares
template <class T> class Ref;
template <class T> class RefConst;

/// Simple class to facilitate reference counting / releasing
/// Derive your class from RefTarget and you can reference it by using Ref<classname> or RefConst<classname>
///
/// Reference counting classes keep an integer which indicates how many references
/// to the object are active. Reference counting objects are derived from RefTarget
/// and staT & their life with a reference count of zero. They can then be assigned
/// to equivalents of pointers (Ref) which will increase the reference count immediately.
/// If the destructor of Ref is called or another object is assigned to the reference
/// counting pointer it will decrease the reference count of the object again. If this
/// reference count becomes zero, the object is destroyed.
///
/// This provides a very powerful mechanism to prevent memory leaks, but also gives
/// some responsibility to the programmer. The most notable point is that you cannot
/// have one object reference another and have the other reference the first one
/// back, because this way the reference count of both objects will never become
/// lower than 1, resulting in a memory leak. By carefully designing your classses
/// (and particularly identifying who owns who in the class hierarchy) you can avoid
/// these problems.
template <class T>
class RefTarget		
{	
public:
	/// Constructor
	inline					RefTarget()										: mRefCount(0) { }
	inline					RefTarget(const RefTarget &)					: mRefCount(0) { }
	inline					~RefTarget()									{ JPH_ASSERT(mRefCount == 0 || mRefCount == cEmbedded); } ///< assert no one is referencing us

	/// Mark this class as embedded, this means the type can be used in a compound or constructed on the stack.
	/// The Release function will never destruct the object, it is assumed the destructor will be called by whoever allocated
	/// the object and at that point in time it is checked that no references are left to the structure.
	inline void				SetEmbedded()									{ JPH_ASSERT(mRefCount < cEmbedded); mRefCount += cEmbedded; }

	/// Assignment operator
	inline RefTarget &		operator = (const RefTarget &inRHS)				{ return *this; }

	/// Get current refcount of this object
	uint32					GetRefCount() const								{ return mRefCount; }

	/// Add or release a reference to this object
	inline void				AddRef() const									{ ++mRefCount; }
	inline void				Release() const									{ if (--mRefCount == 0) delete static_cast<const T *>(this); }

	/// INTERNAL HELPER FUNCTION USED BY SERIALIZATION
	static int				sInternalGetRefCountOffset()					{ return offsetof(T, mRefCount); }

	/// INTERNAL HELPER FUNCTION TO OVERRIDE THE REFCOUNT OF AN OBJECT. USE WITH GREAT CARE! 
	inline void				SetRefCountInternal(uint32 inRefCount)			{ JPH_ASSERT(inRefCount >= 0); mRefCount = inRefCount; }

protected:
	static constexpr uint32 cEmbedded = 0x0ebedded;							///< A large value that gets added to the refcount to mark the object as embedded

	mutable atomic<uint32>	mRefCount;										///< Current reference count
};							

/// Pure virtual version of RefTarget
class RefTargetVirtual
{
public:
	/// Virtual destructor
	virtual					~RefTargetVirtual()								{ }

	/// Virtual add reference
	virtual void			AddRef() = 0;

	/// Virtual release reference
	virtual void			Release() = 0;
};

/// Class for automatic referencing, this is the equivalent of a pointer to type T
/// if you assign a value to this class it will increment the reference count by one
/// of this object, and if you assign something else it will decrease the reference
/// count of the first object again. If it reaches a reference count of zero it will
/// be deleted
template <class T>
class Ref
{
public:
	/// Constructor
	inline					Ref()											: mPtr(nullptr) { }
	inline					Ref(T *inRHS)									: mPtr(inRHS) { AddRef(); }
	inline					Ref(const Ref<T> &inRHS)						: mPtr(inRHS.mPtr) { AddRef(); }
	inline					Ref(Ref<T> &&inRHS) noexcept					: mPtr(inRHS.mPtr) { inRHS.mPtr = nullptr; }
	inline					~Ref()											{ Release(); }
						
	/// Assignment operators
	inline Ref<T> &			operator = (T *inRHS) 							{ if (mPtr != inRHS) { Release(); mPtr = inRHS; AddRef(); } return *this; }
	inline Ref<T> &			operator = (const Ref<T> &inRHS)				{ if (mPtr != inRHS.mPtr) { Release(); mPtr = inRHS.mPtr; AddRef(); } return *this; }
	inline Ref<T> &			operator = (Ref<T> &&inRHS)						{ if (mPtr != inRHS.mPtr) { Release(); mPtr = inRHS.mPtr; inRHS.mPtr = nullptr; } return *this; }
						
	/// Casting operators
	inline					operator T * const () const						{ return mPtr; }
	inline					operator T *()									{ return mPtr; }
						
	/// Access like a normal pointer
	inline T * const 		operator -> () const							{ return mPtr; }
	inline T *				operator -> ()									{ return mPtr; }
	inline T &				operator * () const								{ return *mPtr; }

	/// Comparison
	inline bool				operator == (const T * inRHS) const				{ return mPtr == inRHS; }
	inline bool				operator == (const Ref<T> &inRHS) const			{ return mPtr == inRHS.mPtr; }
	inline bool				operator != (const T * inRHS) const				{ return mPtr != inRHS; }
	inline bool				operator != (const Ref<T> &inRHS) const			{ return mPtr != inRHS.mPtr; }

	/// Get pointer
	inline T * 				GetPtr() const									{ return mPtr; }
	inline T *				GetPtr()										{ return mPtr; }

	/// INTERNAL HELPER FUNCTION USED BY SERIALIZATION
	void **					InternalGetPointer()							{ return reinterpret_cast<void **>(&mPtr); }

private:
	template <class T2> friend class RefConst;

	/// Use "variable = nullptr;" to release an object, do not call these functions
	inline void				AddRef()										{ if (mPtr != nullptr) mPtr->AddRef(); }
	inline void				Release()										{ if (mPtr != nullptr) mPtr->Release(); }
	
	T *						mPtr;											///< Pointer to object that we are reference counting
};						

/// Class for automatic referencing, this is the equivalent of a CONST pointer to type T
/// if you assign a value to this class it will increment the reference count by one
/// of this object, and if you assign something else it will decrease the reference
/// count of the first object again. If it reaches a reference count of zero it will
/// be deleted
template <class T>
class RefConst
{
public:
	/// Constructor
	inline					RefConst()										: mPtr(nullptr) { }
	inline					RefConst(const T * inRHS)						: mPtr(inRHS) { AddRef(); }
	inline					RefConst(const RefConst<T> &inRHS)				: mPtr(inRHS.mPtr) { AddRef(); }
	inline					RefConst(RefConst<T> &&inRHS) noexcept			: mPtr(inRHS.mPtr) { inRHS.mPtr = nullptr; }
	inline					RefConst(const Ref<T> &inRHS)					: mPtr(inRHS.mPtr) { AddRef(); }
	inline					RefConst(Ref<T> &&inRHS)						: mPtr(inRHS.mPtr) { inRHS.mPtr = nullptr; }
	inline					~RefConst()										{ Release(); }
						
	/// Assignment operators
	inline RefConst<T> &	operator = (const T * inRHS) 					{ if (mPtr != inRHS) { Release(); mPtr = inRHS; AddRef(); } return *this; }
	inline RefConst<T> &	operator = (const RefConst<T> &inRHS)			{ if (mPtr != inRHS.mPtr) { Release(); mPtr = inRHS.mPtr; AddRef(); } return *this; }
	inline RefConst<T> &	operator = (RefConst<T> &&inRHS)				{ if (mPtr != inRHS.mPtr) { Release(); mPtr = inRHS.mPtr; inRHS.mPtr = nullptr; } return *this; }
	inline RefConst<T> &	operator = (const Ref<T> &inRHS)				{ if (mPtr != inRHS.mPtr) { Release(); mPtr = inRHS.mPtr; AddRef(); } return *this; }
	inline RefConst<T> &	operator = (Ref<T> &&inRHS)						{ if (mPtr != inRHS.mPtr) { Release(); mPtr = inRHS.mPtr; inRHS.mPtr = nullptr; } return *this; }
						
	/// Casting operators
	inline					operator const T * () const						{ return mPtr; }
						
	/// Access like a normal pointer
	inline const T * 	 	operator -> () const							{ return mPtr; }
	inline const T &		operator * () const								{ return *mPtr; }

	/// Comparison
	inline bool				operator == (const T * inRHS) const				{ return mPtr == inRHS; }
	inline bool				operator == (const RefConst<T> &inRHS) const	{ return mPtr == inRHS.mPtr; }
	inline bool				operator == (const Ref<T> &inRHS) const			{ return mPtr == inRHS.mPtr; }
	inline bool				operator != (const T * inRHS) const				{ return mPtr != inRHS; }
	inline bool				operator != (const RefConst<T> &inRHS) const	{ return mPtr != inRHS.mPtr; }
	inline bool				operator != (const Ref<T> &inRHS) const			{ return mPtr != inRHS.mPtr; }

	/// Get pointer
	inline const T * 		GetPtr() const									{ return mPtr; }

	/// INTERNAL HELPER FUNCTION USED BY SERIALIZATION
	void **					InternalGetPointer()							{ return const_cast<void **>(reinterpret_cast<const void **>(&mPtr)); }

private:
	/// Use "variable = nullptr;" to release an object, do not call these functions
	inline void				AddRef()										{ if (mPtr != nullptr) mPtr->AddRef(); }
	inline void				Release()										{ if (mPtr != nullptr) mPtr->Release(); }
	
	const T *				mPtr;											///< Pointer to object that we are reference counting
};						

} // JPH

namespace std
{
	/// Declare std::hash for Ref
	template <class T> 
	struct hash<JPH::Ref<T>>
	{
		size_t operator () (const JPH::Ref<T> &inRHS) const
		{
			return hash<T *> { }(inRHS.GetPtr());
		}
	};

	/// Declare std::hash for RefConst
	template <class T> 
	struct hash<JPH::RefConst<T>>
	{
		size_t operator () (const JPH::RefConst<T> &inRHS) const
		{
			return hash<const T *> { }(inRHS.GetPtr());
		}
	};
}
