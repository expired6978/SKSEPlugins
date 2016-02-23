//--------------------------------------------------------------------------------------
// File: MeshLoader.h
//
// Wrapper class for ID3DXMesh interface. Handles loading mesh data from an .obj file
// and resource management for material textures.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef _MESHLOADER_H_
#define _MESHLOADER_H_
#pragma once


#include <crtdefs.h>

#undef  assert

#ifdef  NDEBUG

#define assert(_Expression)     ((void)0)

#else

#ifdef  __cplusplus
extern "C" {
#endif

	_CRTIMP void __cdecl _wassert(_In_z_ const wchar_t * _Message, _In_z_ const wchar_t *_File, _In_ unsigned _Line);

#ifdef  __cplusplus
}
#endif

#define assert(_Expression) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), __LINE__), 0) )

#endif  /* NDEBUG */


#include <d3dx9.h>
#include <windows.h>


#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x)           x
#endif
#ifndef V_RETURN
#define V_RETURN(x)   x
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif    
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#endif    
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

template<typename TYPE> class CGrowableArray
{
public:
	CGrowableArray()
	{
		m_pData = NULL; m_nSize = 0; m_nMaxSize = 0;
	}
	CGrowableArray( const CGrowableArray <TYPE>& a )
	{
		for( int i = 0; i < a.m_nSize; i++ ) Add( a.m_pData[i] );
	}
	~CGrowableArray()
	{
		RemoveAll();
	}

	const TYPE& operator[]( int nIndex ) const
	{
		return GetAt( nIndex );
	}
	TYPE& operator[]( int nIndex )
	{
		return GetAt( nIndex );
	}

	CGrowableArray& operator=( const CGrowableArray <TYPE>& a )
	{
		if( this == &a ) return *this; RemoveAll(); for( int i = 0; i < a.m_nSize;
		i++ ) Add( a.m_pData[i] ); return *this;
	}

	HRESULT SetSize( int nNewMaxSize );
	HRESULT Add( const TYPE& value );
	HRESULT Insert( int nIndex, const TYPE& value );
	HRESULT SetAt( int nIndex, const TYPE& value );
	TYPE& GetAt( int nIndex ) const
	{
		if( nIndex >= 0 && nIndex < m_nSize ) return m_pData[nIndex];
	}
	int     GetSize() const
	{
		return m_nSize;
	}
	TYPE* GetData()
	{
		return m_pData;
	}
	bool    Contains( const TYPE& value )
	{
		return ( -1 != IndexOf( value ) );
	}

	int     IndexOf( const TYPE& value )
	{
		return ( m_nSize > 0 ) ? IndexOf( value, 0, m_nSize ) : -1;
	}
	int     IndexOf( const TYPE& value, int iStart )
	{
		return IndexOf( value, iStart, m_nSize - iStart );
	}
	int     IndexOf( const TYPE& value, int nIndex, int nNumElements );

	int     LastIndexOf( const TYPE& value )
	{
		return ( m_nSize > 0 ) ? LastIndexOf( value, m_nSize - 1, m_nSize ) : -1;
	}
	int     LastIndexOf( const TYPE& value, int nIndex )
	{
		return LastIndexOf( value, nIndex, nIndex + 1 );
	}
	int     LastIndexOf( const TYPE& value, int nIndex, int nNumElements );

	HRESULT Remove( int nIndex );
	void    RemoveAll()
	{
		SetSize( 0 );
	}
	void    Reset()
	{
		m_nSize = 0;
	}

protected:
	TYPE* m_pData;      // the actual array of data
	int m_nSize;        // # of elements (upperBound - 1)
	int m_nMaxSize;     // max allocated

	HRESULT SetSizeInternal( int nNewMaxSize );  // This version doesn't call ctor or dtor.
};

// Vertex format
struct VERTEX
{
    D3DXVECTOR3 position;
    D3DXVECTOR3 normal;
    D3DXVECTOR2 texcoord;
};


// Used for a hashtable vertex cache when creating the mesh from a .obj file
struct CacheEntry
{
    UINT index;
    CacheEntry* pNext;
};


// Material properties per mesh subset
struct Material
{
    WCHAR   strName[MAX_PATH];

    D3DXVECTOR3 vAmbient;
    D3DXVECTOR3 vDiffuse;
    D3DXVECTOR3 vSpecular;

    int nShininess;
    float fAlpha;

    bool bSpecular;

    WCHAR   strTexture[MAX_PATH];
    IDirect3DTexture9* pTexture;
    D3DXHANDLE hTechnique;
};


class CMeshLoader
{
public:
            CMeshLoader();
            ~CMeshLoader();

    HRESULT Create( IDirect3DDevice9* pd3dDevice, const WCHAR* strFilename );
    void    Destroy();


    UINT    GetNumMaterials() const
    {
        return m_Materials.GetSize();
    }
    Material* GetMaterial( UINT iMaterial )
    {
        return m_Materials.GetAt( iMaterial );
    }

    ID3DXMesh* GetMesh()
    {
        return m_pMesh;
    }
    WCHAR* GetMediaDirectory()
    {
        return m_strMediaDir;
    }

private:

    HRESULT LoadGeometryFromOBJ( const WCHAR* strFilename );
    void    InitMaterial( Material* pMaterial );

    DWORD   AddVertex( UINT hash, VERTEX* pVertex );
    void    DeleteCache();

    IDirect3DDevice9* m_pd3dDevice;    // Direct3D Device object associated with this mesh
    ID3DXMesh* m_pMesh;         // Encapsulated D3DX Mesh

    CGrowableArray <CacheEntry*> m_VertexCache;   // Hashtable cache for locating duplicate vertices
    CGrowableArray <VERTEX> m_Vertices;      // Filled and copied to the vertex buffer
    CGrowableArray <DWORD> m_Indices;       // Filled and copied to the index buffer
    CGrowableArray <DWORD> m_Attributes;    // Filled and copied to the attribute buffer
    CGrowableArray <Material*> m_Materials;     // Holds material properties per subset

    WCHAR   m_strMediaDir[ MAX_PATH ];               // Directory where the mesh was found
};

#endif // _MESHLOADER_H_

