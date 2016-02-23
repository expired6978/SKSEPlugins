//--------------------------------------------------------------------------------------
// File: MeshLoader.cpp
//
// Wrapper class for ID3DXMesh interface. Handles loading mesh data from an .obj file
// and resource management for material textures.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma warning(disable: 4995)
#include "meshloader.h"
#include <fstream>
using namespace std;
#pragma warning(default: 4995)


//--------------------------------------------------------------------------------------
// Implementation of CGrowableArray
//--------------------------------------------------------------------------------------

// This version doesn't call ctor or dtor.
template<typename TYPE> HRESULT CGrowableArray <TYPE>::SetSizeInternal( int nNewMaxSize )
{
	if( nNewMaxSize < 0 || ( nNewMaxSize > INT_MAX / sizeof( TYPE ) ) )
	{
		assert( false );
		return E_INVALIDARG;
	}

	if( nNewMaxSize == 0 )
	{
		// Shrink to 0 size & cleanup
		if( m_pData )
		{
			free( m_pData );
			m_pData = NULL;
		}

		m_nMaxSize = 0;
		m_nSize = 0;
	}
	else if( m_pData == NULL || nNewMaxSize > m_nMaxSize )
	{
		// Grow array
		int nGrowBy = ( m_nMaxSize == 0 ) ? 16 : m_nMaxSize;

		// Limit nGrowBy to keep m_nMaxSize less than INT_MAX
		if( ( UINT )m_nMaxSize + ( UINT )nGrowBy > ( UINT )INT_MAX )
			nGrowBy = INT_MAX - m_nMaxSize;

		nNewMaxSize = __max( nNewMaxSize, m_nMaxSize + nGrowBy );

		// Verify that (nNewMaxSize * sizeof(TYPE)) is not greater than UINT_MAX or the realloc will overrun
		if( sizeof( TYPE ) > UINT_MAX / ( UINT )nNewMaxSize )
			return E_INVALIDARG;

		TYPE* pDataNew = ( TYPE* )realloc( m_pData, nNewMaxSize * sizeof( TYPE ) );
		if( pDataNew == NULL )
			return E_OUTOFMEMORY;

		m_pData = pDataNew;
		m_nMaxSize = nNewMaxSize;
	}

	return S_OK;
}


//--------------------------------------------------------------------------------------
template<typename TYPE> HRESULT CGrowableArray <TYPE>::SetSize( int nNewMaxSize )
{
	int nOldSize = m_nSize;

	if( nOldSize > nNewMaxSize )
	{
		assert( m_pData );
		if( m_pData )
		{
			// Removing elements. Call dtor.

			for( int i = nNewMaxSize; i < nOldSize; ++i )
				m_pData[i].~TYPE();
		}
	}

	// Adjust buffer.  Note that there's no need to check for error
	// since if it happens, nOldSize == nNewMaxSize will be true.)
	HRESULT hr = SetSizeInternal( nNewMaxSize );

	if( nOldSize < nNewMaxSize )
	{
		assert( m_pData );
		if( m_pData )
		{
			// Adding elements. Call ctor.

			for( int i = nOldSize; i < nNewMaxSize; ++i )
				::new ( &m_pData[i] ) TYPE;
		}
	}

	return hr;
}


//--------------------------------------------------------------------------------------
template<typename TYPE> HRESULT CGrowableArray <TYPE>::Add( const TYPE& value )
{
	HRESULT hr;
	if( FAILED( hr = SetSizeInternal( m_nSize + 1 ) ) )
		return hr;

	// Construct the new element
	::new ( &m_pData[m_nSize] ) TYPE;

	// Assign
	m_pData[m_nSize] = value;
	++m_nSize;

	return S_OK;
}


//--------------------------------------------------------------------------------------
template<typename TYPE> HRESULT CGrowableArray <TYPE>::Insert( int nIndex, const TYPE& value )
{
	HRESULT hr;

	// Validate index
	if( nIndex < 0 ||
		nIndex > m_nSize )
	{
		assert( false );
		return E_INVALIDARG;
	}

	// Prepare the buffer
	if( FAILED( hr = SetSizeInternal( m_nSize + 1 ) ) )
		return hr;

	// Shift the array
	MoveMemory( &m_pData[nIndex + 1], &m_pData[nIndex], sizeof( TYPE ) * ( m_nSize - nIndex ) );

	// Construct the new element
	::new ( &m_pData[nIndex] ) TYPE;

	// Set the value and increase the size
	m_pData[nIndex] = value;
	++m_nSize;

	return S_OK;
}


//--------------------------------------------------------------------------------------
template<typename TYPE> HRESULT CGrowableArray <TYPE>::SetAt( int nIndex, const TYPE& value )
{
	// Validate arguments
	if( nIndex < 0 ||
		nIndex >= m_nSize )
	{
		assert( false );
		return E_INVALIDARG;
	}

	m_pData[nIndex] = value;
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Searches for the specified value and returns the index of the first occurrence
// within the section of the data array that extends from iStart and contains the 
// specified number of elements. Returns -1 if value is not found within the given 
// section.
//--------------------------------------------------------------------------------------
template<typename TYPE> int CGrowableArray <TYPE>::IndexOf( const TYPE& value, int iStart, int nNumElements )
{
	// Validate arguments
	if( iStart < 0 ||
		iStart >= m_nSize ||
		nNumElements < 0 ||
		iStart + nNumElements > m_nSize )
	{
		assert( false );
		return -1;
	}

	// Search
	for( int i = iStart; i < ( iStart + nNumElements ); i++ )
	{
		if( value == m_pData[i] )
			return i;
	}

	// Not found
	return -1;
}


//--------------------------------------------------------------------------------------
// Searches for the specified value and returns the index of the last occurrence
// within the section of the data array that contains the specified number of elements
// and ends at iEnd. Returns -1 if value is not found within the given section.
//--------------------------------------------------------------------------------------
template<typename TYPE> int CGrowableArray <TYPE>::LastIndexOf( const TYPE& value, int iEnd, int nNumElements )
{
	// Validate arguments
	if( iEnd < 0 ||
		iEnd >= m_nSize ||
		nNumElements < 0 ||
		iEnd - nNumElements < 0 )
	{
		assert( false );
		return -1;
	}

	// Search
	for( int i = iEnd; i > ( iEnd - nNumElements ); i-- )
	{
		if( value == m_pData[i] )
			return i;
	}

	// Not found
	return -1;
}



//--------------------------------------------------------------------------------------
template<typename TYPE> HRESULT CGrowableArray <TYPE>::Remove( int nIndex )
{
	if( nIndex < 0 ||
		nIndex >= m_nSize )
	{
		assert( false );
		return E_INVALIDARG;
	}

	// Destruct the element to be removed
	m_pData[nIndex].~TYPE();

	// Compact the array and decrease the size
	MoveMemory( &m_pData[nIndex], &m_pData[nIndex + 1], sizeof( TYPE ) * ( m_nSize - ( nIndex + 1 ) ) );
	--m_nSize;

	return S_OK;
}


// Vertex declaration
D3DVERTEXELEMENT9 VERTEX_DECL[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0},
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0},
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
CMeshLoader::CMeshLoader()
{
    m_pd3dDevice = NULL;
    m_pMesh = NULL;

    ZeroMemory( m_strMediaDir, sizeof( m_strMediaDir ) );
}


//--------------------------------------------------------------------------------------
CMeshLoader::~CMeshLoader()
{
    Destroy();
}


//--------------------------------------------------------------------------------------
void CMeshLoader::Destroy()
{
    for( int iMaterial = 0; iMaterial < m_Materials.GetSize(); iMaterial++ )
    {
        Material* pMaterial = m_Materials.GetAt( iMaterial );

        // Avoid releasing the same texture twice
        for( int x = iMaterial + 1; x < m_Materials.GetSize(); x++ )
        {
            Material* pCur = m_Materials.GetAt( x );
            if( pCur->pTexture == pMaterial->pTexture )
                pCur->pTexture = NULL;
        }

        SAFE_RELEASE( pMaterial->pTexture );
        SAFE_DELETE( pMaterial );
    }

    m_Materials.RemoveAll();
    m_Vertices.RemoveAll();
    m_Indices.RemoveAll();
    m_Attributes.RemoveAll();

    SAFE_RELEASE( m_pMesh );
    m_pd3dDevice = NULL;
}


//--------------------------------------------------------------------------------------
HRESULT CMeshLoader::Create( IDirect3DDevice9* pd3dDevice, const WCHAR* strFilename )
{
    WCHAR str[ MAX_PATH ] = {0};

    // Start clean
    Destroy();

    // Store the device pointer
    m_pd3dDevice = pd3dDevice;

    // Load the vertex buffer, index buffer, and subset information from a file. In this case, 
    // an .obj file was chosen for simplicity, but it's meant to illustrate that ID3DXMesh objects
    // can be filled from any mesh file format once the necessary data is extracted from file.
    LoadGeometryFromOBJ( strFilename );

    // Create the encapsulated mesh
    ID3DXMesh* pMesh = NULL;
    V_RETURN( D3DXCreateMesh( m_Indices.GetSize() / 3, m_Vertices.GetSize(),
                              D3DXMESH_MANAGED | D3DXMESH_32BIT, VERTEX_DECL,
                              pd3dDevice, &pMesh ) );

    // Copy the vertex data
    VERTEX* pVertex;
    V_RETURN( pMesh->LockVertexBuffer( 0, ( void** )&pVertex ) );
    memcpy( pVertex, m_Vertices.GetData(), m_Vertices.GetSize() * sizeof( VERTEX ) );
    pMesh->UnlockVertexBuffer();
    m_Vertices.RemoveAll();

    // Copy the index data
    DWORD* pIndex;
    V_RETURN( pMesh->LockIndexBuffer( 0, ( void** )&pIndex ) );
    memcpy( pIndex, m_Indices.GetData(), m_Indices.GetSize() * sizeof( DWORD ) );
    pMesh->UnlockIndexBuffer();
    m_Indices.RemoveAll();

    // Copy the attribute data
    DWORD* pSubset;
    V_RETURN( pMesh->LockAttributeBuffer( 0, &pSubset ) );
    memcpy( pSubset, m_Attributes.GetData(), m_Attributes.GetSize() * sizeof( DWORD ) );
    pMesh->UnlockAttributeBuffer();
    m_Attributes.RemoveAll();

    // Reorder the vertices according to subset and optimize the mesh for this graphics 
    // card's vertex cache. When rendering the mesh's triangle list the vertices will 
    // cache hit more often so it won't have to re-execute the vertex shader.
    DWORD* aAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
    if( aAdjacency == NULL )
        return E_OUTOFMEMORY;

    V( pMesh->GenerateAdjacency( 1e-6f, aAdjacency ) );
    V( pMesh->OptimizeInplace( D3DXMESHOPT_ATTRSORT | D3DXMESHOPT_VERTEXCACHE, aAdjacency, NULL, NULL, NULL ) );

    SAFE_DELETE_ARRAY( aAdjacency );
    m_pMesh = pMesh;

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CMeshLoader::LoadGeometryFromOBJ( const WCHAR* strFileName )
{
    WCHAR strMaterialFilename[MAX_PATH] = {0};
    char str[MAX_PATH];

    WideCharToMultiByte( CP_ACP, 0, strFileName, -1, str, MAX_PATH, NULL, NULL );

    // Create temporary storage for the input data. Once the data has been loaded into
    // a reasonable format we can create a D3DXMesh object and load it with the mesh data.
    CGrowableArray <D3DXVECTOR3> Positions;
    CGrowableArray <D3DXVECTOR2> TexCoords;
    CGrowableArray <D3DXVECTOR3> Normals;

    DWORD dwCurSubset = 0;

    // File input
    WCHAR strCommand[256] = {0};
    wifstream InFile( str );
    if( !InFile )
        return NULL;

    for(; ; )
    {
        InFile >> strCommand;
        if( !InFile )
            break;

        if( 0 == wcscmp( strCommand, L"#" ) )
        {
            // Comment
        }
        else if( 0 == wcscmp( strCommand, L"v" ) )
        {
            // Vertex Position
            float x, y, z;
            InFile >> x >> y >> z;
            Positions.Add( D3DXVECTOR3( x, y, z ) );
        }
        else if( 0 == wcscmp( strCommand, L"vt" ) )
        {
            // Vertex TexCoord
            float u, v;
            InFile >> u >> v;
            TexCoords.Add( D3DXVECTOR2( u, v ) );
        }
        else if( 0 == wcscmp( strCommand, L"vn" ) )
        {
            // Vertex Normal
            float x, y, z;
            InFile >> x >> y >> z;
            Normals.Add( D3DXVECTOR3( x, y, z ) );
        }
        else if( 0 == wcscmp( strCommand, L"f" ) )
        {
            // Face
            UINT iPosition, iTexCoord, iNormal;
            VERTEX vertex;

            for( UINT iFace = 0; iFace < 3; iFace++ )
            {
                ZeroMemory( &vertex, sizeof( VERTEX ) );

                // OBJ format uses 1-based arrays
                InFile >> iPosition;
                vertex.position = Positions[ iPosition - 1 ];

                if( '/' == InFile.peek() )
                {
                    InFile.ignore();

                    if( '/' != InFile.peek() )
                    {
                        // Optional texture coordinate
                        InFile >> iTexCoord;
                        vertex.texcoord = TexCoords[ iTexCoord - 1 ];
                    }

                    if( '/' == InFile.peek() )
                    {
                        InFile.ignore();

                        // Optional vertex normal
                        InFile >> iNormal;
                        vertex.normal = Normals[ iNormal - 1 ];
                    }
                }

                // If a duplicate vertex doesn't exist, add this vertex to the Vertices
                // list. Store the index in the Indices array. The Vertices and Indices
                // lists will eventually become the Vertex Buffer and Index Buffer for
                // the mesh.
                DWORD index = AddVertex( iPosition, &vertex );
                m_Indices.Add( index );
            }
            m_Attributes.Add( dwCurSubset );
        }
        
        InFile.ignore( 1000, '\n' );
    }

    // Cleanup
    InFile.close();
    DeleteCache();

    return S_OK;
}


//--------------------------------------------------------------------------------------
DWORD CMeshLoader::AddVertex( UINT hash, VERTEX* pVertex )
{
    // If this vertex doesn't already exist in the Vertices list, create a new entry.
    // Add the index of the vertex to the Indices list.
    bool bFoundInList = false;
    DWORD index = 0;

    // Since it's very slow to check every element in the vertex list, a hashtable stores
    // vertex indices according to the vertex position's index as reported by the OBJ file
    if( ( UINT )m_VertexCache.GetSize() > hash )
    {
        CacheEntry* pEntry = m_VertexCache.GetAt( hash );
        while( pEntry != NULL )
        {
            VERTEX* pCacheVertex = m_Vertices.GetData() + pEntry->index;

            // If this vertex is identical to the vertex already in the list, simply
            // point the index buffer to the existing vertex
            if( 0 == memcmp( pVertex, pCacheVertex, sizeof( VERTEX ) ) )
            {
                bFoundInList = true;
                index = pEntry->index;
                break;
            }

            pEntry = pEntry->pNext;
        }
    }

    // Vertex was not found in the list. Create a new entry, both within the Vertices list
    // and also within the hashtable cache
    if( !bFoundInList )
    {
        // Add to the Vertices list
        index = m_Vertices.GetSize();
        m_Vertices.Add( *pVertex );

        // Add this to the hashtable
        CacheEntry* pNewEntry = new CacheEntry;
        if( pNewEntry == NULL )
            return E_OUTOFMEMORY;

        pNewEntry->index = index;
        pNewEntry->pNext = NULL;

        // Grow the cache if needed
        while( ( UINT )m_VertexCache.GetSize() <= hash )
        {
            m_VertexCache.Add( NULL );
        }

        // Add to the end of the linked list
        CacheEntry* pCurEntry = m_VertexCache.GetAt( hash );
        if( pCurEntry == NULL )
        {
            // This is the head element
            m_VertexCache.SetAt( hash, pNewEntry );
        }
        else
        {
            // Find the tail
            while( pCurEntry->pNext != NULL )
            {
                pCurEntry = pCurEntry->pNext;
            }

            pCurEntry->pNext = pNewEntry;
        }
    }

    return index;
}


//--------------------------------------------------------------------------------------
void CMeshLoader::DeleteCache()
{
    // Iterate through all the elements in the cache and subsequent linked lists
    for( int i = 0; i < m_VertexCache.GetSize(); i++ )
    {
        CacheEntry* pEntry = m_VertexCache.GetAt( i );
        while( pEntry != NULL )
        {
            CacheEntry* pNext = pEntry->pNext;
            SAFE_DELETE( pEntry );
            pEntry = pNext;
        }
    }

    m_VertexCache.RemoveAll();
}

//--------------------------------------------------------------------------------------
void CMeshLoader::InitMaterial( Material* pMaterial )
{
    ZeroMemory( pMaterial, sizeof( Material ) );

    pMaterial->vAmbient = D3DXVECTOR3( 0.2f, 0.2f, 0.2f );
    pMaterial->vDiffuse = D3DXVECTOR3( 0.8f, 0.8f, 0.8f );
    pMaterial->vSpecular = D3DXVECTOR3( 1.0f, 1.0f, 1.0f );
    pMaterial->nShininess = 0;
    pMaterial->fAlpha = 1.0f;
    pMaterial->bSpecular = false;
    pMaterial->pTexture = NULL;
}
