/** Copyright (C) 2013 Robert B. Colton, Adriano Tumminelli
*** Copyright (C) 2014 Josh Ventura, Harijs Grinbergs
***
*** This file is a part of the ENIGMA Development Environment.
***
*** ENIGMA is free software: you can redistribute it and/or modify it under the
*** terms of the GNU General Public License as published by the Free Software
*** Foundation, version 3 of the license or any later version.
***
*** This application and its source code is distributed AS-IS, WITHOUT ANY
*** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
*** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
*** details.
***
*** You should have received a copy of the GNU General Public License along
*** with this code. If not, see <http://www.gnu.org/licenses/>
**/
#include "../General/OpenGLHeaders.h"
#include "../General/GSd3d.h"
#include "../General/GSprimitives.h"
#include "../General/GSmatrix.h"
#include "Bridges/General/GL3Context.h" //Needed to get if bound texture == -1
#include "GLSLshader.h"
#include "Universal_System/var4.h"
#include "Universal_System/roomsystem.h"
#include "GL3shader.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#define __GETR(x) (x & 0x0000FF)
#define __GETG(x) ((x & 0x00FF00)>>8)
#define __GETB(x) ((x & 0xFF0000)>>16)

#include <iostream>
#include <map>
#include <list>
#include "Universal_System/fileio.h"
#include "Universal_System/estring.h"

#include <vector>
using std::vector;

extern GLenum ptypes_by_id[16];
namespace enigma {
  extern unsigned char currentcolor[4];
  extern unsigned bound_shader;
  extern vector<enigma::ShaderProgram*> shaderprograms;

  //split a string and convert to float
  vector<float> float_split(const string& str, const char& ch) {
    string next;
    vector<float> result;

    for (string::const_iterator it = str.begin(); it != str.end(); it++)
	{
		if (*it == ch)
		{
			if (!next.empty())
			{
				result.push_back(atof(next.c_str()));
				next.clear();
			}
        } else {
            next += *it;
        }
    }
    if (!next.empty())
         result.push_back(atof(next.c_str()));
    return result;
  }

  //obj model parsing functions
  void string_parse( string *s )
  {
	size_t spaces = 0;
	bool trimmed = false;
	bool checknormal = false;
	for (unsigned int i = 0; i < s->size() ; i++)
	{
		//comment
		if ((*s)[i] == '#')
		{
			s->erase(i, s->length() - i);
			break;
		}
		else if((*s)[i] == ' ')
		{
			if (!trimmed)
			{
				s->erase(i,1);
				i--;
			}
			else
			{
				if (spaces >= 1)
				{
					s->erase(i,1);
					i--;
				}
				spaces++;
			}
		}
		else
		{
			if((*s)[i] == '/')
			{
				(*s)[i] = ' ';
				if(checknormal)
				{
					s->erase(i, 1);
					checknormal = false;
				}
				else
					checknormal = true;
			}
			else
				checknormal = false;
			spaces = 0;
			trimmed = true;
		}
	}
	//end trim
	if (s->size() > 0) {
		if ((*s)[s->size()-1] == ' ')
		{
			s->erase(s->size()-1, 1);
		}
	}
  }
}

template<int x> struct intmatch { };
template<int x> struct uintmatch { };
template<> struct intmatch<1>   { typedef int8_t type;  };
template<> struct intmatch<2>   { typedef int16_t type; };
template<> struct intmatch<4>   { typedef int32_t type; };
template<> struct intmatch<8>   { typedef int64_t type; };
template<> struct uintmatch<1>  { typedef uint8_t type;  };
template<> struct uintmatch<2>  { typedef uint16_t type; };
template<> struct uintmatch<4>  { typedef uint32_t type; };
template<> struct uintmatch<8>  { typedef uint64_t type; };
typedef uintmatch<sizeof(gs_scalar)>::type color_t;
union VertexElement {
	color_t d;
	gs_scalar f;

	VertexElement(gs_scalar v): f(v) {}
	VertexElement(color_t v): d(v) {}
};

//NOTE: This class handles batching, indexing, and other optimization for you and is very very efficient.
class Mesh
{
  public:
  unsigned currentPrimitive; //The type of the current primitive being added to the model

  vector<VertexElement> vertices; // Temporary vertices container for the current primitive until they are batched
  vector<GLuint> indices; // Temporary indices that can optionally be supplied, otherwise they will get generated by the batcher.
  vector<VertexElement> triangleVertices; // The vertices added to triangle primitives batched into a single triangle list to be buffered to the GPU
  vector<VertexElement> triangleIndexedVertices; // The vertices added to indexed triangle primitives batched into a single triangle list to be buffered to the GPU
  vector<GLuint> triangleIndices; // The triangle indices either concatenated by batching or supplied in the temporary container.
  vector<VertexElement> lineVertices; // The vertices added to line primitives batched into a single line list to be buffered to the GPU
  vector<VertexElement> lineIndexedVertices; // The vertices added to indexed line primitives batched into a single line list to be buffered to the GPU
  vector<GLuint> lineIndices; // The line indices either concatenated by batching or supplied in the temporary container.
  vector<VertexElement> pointVertices; // The vertices added to point primitives batched into a single point list to be buffered to the GPU
  vector<VertexElement> pointIndexedVertices; // The vertices added to indexed point primitives batched into a single point list to be buffered to the GPU
  vector<GLuint> pointIndices; // The point indices either concatenated by batching or supplied in the temporary container.

  unsigned vertexStride; // whether the vertices are 2D or 3D
  bool useColors; // If colors have been added to the model
  bool useTextures; // If texture coordinates have been added
  bool useNormals; // If normals have been added

  unsigned pointCount; // The number of vertices in the point buffer
  unsigned triangleCount; // The number of vertices in the triangle buffer
  unsigned lineCount; // The number of vertices in the line buffer

  unsigned indexedoffset; // The number of indexed vertices
  unsigned pointIndexedCount; // The number of point indices
  unsigned triangleIndexedCount; // The number of triangle indices
  unsigned lineIndexedCount; // The number of line indices

  unsigned vbufferSize; //Current VBO size
  unsigned ibufferSize; //Current index buffer size

  // Indexed primitives are first since the indices must be offset, and keeps them as small as possible.
  // INDEXEDTRIANGLES|INDEXEDLINES|INDEXEDPOINTS|TRIANGLES|LINES|POINTS
  GLuint vertexBuffer; // Interleaved vertex buffer object with triangles first since they are most likely to be used
  GLuint indexBuffer; // Interleaved index buffer object with triangles first since they are most likely to be used

  int vbotype; // Can be static = GL_STATIC_DRAW, dynamic = GL_DYNAMIC_DRAW or stream GL_STREAM_DRAW
  bool ibogenerated;
  bool vbogenerated;
  bool vbobuffered; // Whether or not the buffer objects have been generated
  bool vboindexed; // Whether or not the model contains any indexed primitives or just regular lists

  Mesh (int type)
  {
    triangleIndexedVertices.reserve(64000);
    pointIndexedVertices.reserve(64000);
    lineIndexedVertices.reserve(64000);
    pointVertices.reserve(64000);
    pointIndices.reserve(64000);
    lineVertices.reserve(64000);
    lineIndices.reserve(64000);
    triangleVertices.reserve(64000);
    triangleIndices.reserve(64000);
    vertices.reserve(64000);
    indices.reserve(64000);

    switch (type){
      case model_static: vbotype = GL_STATIC_DRAW; break;
      case model_dynamic: vbotype = GL_DYNAMIC_DRAW; break;
      case model_stream: vbotype = GL_STREAM_DRAW; break;
    }

    ibogenerated = false;
    vbogenerated = false;
    vbobuffered = false;
    vboindexed = false;

    vertexStride = 0;
    useColors = false;
    useTextures = false;
    useNormals = false;

    pointCount = 0;
    triangleCount = 0;
    lineCount = 0;

    indexedoffset = 0;
    pointIndexedCount = 0;
    triangleIndexedCount = 0;
    lineIndexedCount = 0;

    currentPrimitive = 0;
    vbufferSize = 0;
    ibufferSize = 0;
  }

  ~Mesh()
  {
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteBuffers(1, &indexBuffer);
  }

  void ClearData()
  {
    triangleVertices.clear();
    pointVertices.clear();
    lineVertices.clear();
    triangleIndexedVertices.clear();
    pointIndexedVertices.clear();
    lineIndexedVertices.clear();
    triangleIndices.clear();
    pointIndices.clear();
    lineIndices.clear();
    vertices.clear();
    indices.clear();
  }

  void Clear()
  {
    ClearData();

    triangleIndexedVertices.reserve(64000);
    pointIndexedVertices.reserve(64000);
    lineIndexedVertices.reserve(64000);
    pointVertices.reserve(64000);
    pointIndices.reserve(64000);
    lineVertices.reserve(64000);
    lineIndices.reserve(64000);
    triangleVertices.reserve(64000);
    triangleIndices.reserve(64000);
    vertices.reserve(64000);
    indices.reserve(64000);

    vbobuffered = false;
    vboindexed = false;

    vertexStride = 0;
    useColors = false;
    useTextures = false;
    useNormals = false;

    pointCount = 0;
    triangleCount = 0;
    lineCount = 0;
    indexedoffset = 0;
    pointIndexedCount = 0;
    triangleIndexedCount = 0;
    lineIndexedCount = 0;
  }

  unsigned GetStride() {
    unsigned stride = vertexStride;
    if (useNormals) stride += 3;
    if (useTextures) stride += 2;
    if (useColors) stride += 1;
    return stride;
  }

  void Begin(int pt)
  {
    vbobuffered = false;
    currentPrimitive = pt;
  }

  void AddVertex(gs_scalar x, gs_scalar y)
  {
    vertices.push_back(x); vertices.push_back(y);
    vertexStride = 2;
  }

  void AddVertex(gs_scalar x, gs_scalar y, gs_scalar z)
  {
    vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
    vertexStride = 3;
  }

  void AddIndex(unsigned ind)
  {
    indices.push_back(ind);
  }

  void AddNormal(gs_scalar nx, gs_scalar ny, gs_scalar nz)
  {
    vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
    useNormals = true;
  }

  void AddTexture(gs_scalar tx, gs_scalar ty)
  {
    vertices.push_back(tx); vertices.push_back(ty);
    useTextures = true;
  }

  void AddColor(int col, double alpha)
  {
    color_t finalcol = col + ((unsigned char)(alpha*255) << 24);
    vertices.push_back(finalcol);
    useColors = true;
  }

  void End()
  {
	//NOTE: This batching only checks for degenerate primitives on triangle strips and fans since the GPU does not render triangles where the two
	//vertices are exactly the same, triangle lists could also check for degenerates, it is unknown whether the GPU will render a degenerative
	//in a line strip primitive.

	unsigned stride = GetStride();
	if (vertices.size() == 0) return;

	// Primitive has ended so now we need to batch the vertices that were given into single lists, eg. line list, triangle list, point list
	// Indices are optionally supplied, model functions can also be added for the end user to supply the indexing themselves for each primitive
	// but the batching system does not care either way if they are not supplied it will automatically generate them.
	switch (currentPrimitive) {
		case enigma_user::pr_pointlist:
			if (indices.size() > 0) {
				pointIndexedVertices.insert(pointIndexedVertices.end(), vertices.begin(), vertices.end());
				for (std::vector<GLuint>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += pointIndexedCount; }
				pointIndices.insert(pointIndices.end(), indices.begin(), indices.end());
			} else {
				pointVertices.insert(pointVertices.end(), vertices.begin(), vertices.end());
				pointCount += vertices.size() / stride;
			}
			break;
		case enigma_user::pr_linelist:
			if (indices.size() > 0) {
				lineIndexedVertices.insert(lineIndexedVertices.end(), vertices.begin(), vertices.end());
				for (std::vector<GLuint>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += lineIndexedCount; }
				lineIndices.insert(lineIndices.end(), indices.begin(), indices.end());
			} else {
				lineVertices.insert(lineVertices.end(), vertices.begin(), vertices.end());
				lineCount += vertices.size() / stride;
			}
			break;
		case enigma_user::pr_linestrip:
			lineIndexedVertices.insert(lineIndexedVertices.end(), vertices.begin(), vertices.end());
			if (indices.size() > 0) {
				for (std::vector<GLuint>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += lineIndexedCount; }
				for (unsigned i = 0; i < indices.size() - 2; i++) {
					lineIndices.push_back(indices[i]);
					lineIndices.push_back(indices[i + 1]);
				}
			} else {
				unsigned offset = (lineIndexedVertices.size() - vertices.size()) / stride;
				for (unsigned i = 0; i < vertices.size() / stride - 1; i++) {
					lineIndices.push_back(offset + i);
					lineIndices.push_back(offset + i + 1);
				}
			}
			break;
		case enigma_user::pr_trianglelist:
			if (indices.size() > 0) {
				triangleIndexedVertices.insert(triangleIndexedVertices.end(), vertices.begin(), vertices.end());
				for (std::vector<GLuint>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += triangleIndexedCount; }
				triangleIndices.insert(triangleIndices.end(), indices.begin(), indices.end());
			} else {
				triangleVertices.insert(triangleVertices.end(), vertices.begin(), vertices.end());
				triangleCount += vertices.size() / stride;
			}
			break;
		case enigma_user::pr_trianglestrip:
			triangleIndexedVertices.insert(triangleIndexedVertices.end(), vertices.begin(), vertices.end());
			if (indices.size() > 0) {
				for (std::vector<GLuint>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += triangleIndexedCount; }
				for (unsigned i = 0; i < indices.size() - 2; i++) {
					// check for and continue if indexed triangle is degenerate, because the GPU won't render it anyway
					if (indices[i] == indices[i + 1] || indices[i] == indices[i + 2]  || indices[i + 1] == indices[i + 2] ) { continue; }
					if (i % 2) {
                        triangleIndices.push_back(indices[i+1]);
                        triangleIndices.push_back(indices[i]);
                        triangleIndices.push_back(indices[i+2]);
					}else{
                        triangleIndices.push_back(indices[i]);
                        triangleIndices.push_back(indices[i+1]);
                        triangleIndices.push_back(indices[i+2]);
					}
				}
			} else {
				unsigned offset = (triangleIndexedVertices.size() - vertices.size()) / stride;
				for (unsigned i = 0; i < vertices.size() / stride - 2; i++) {
					if (i % 2) {
						triangleIndices.push_back(offset + i + 2);
						triangleIndices.push_back(offset + i + 1);
						triangleIndices.push_back(offset + i);
					} else {
						triangleIndices.push_back(offset + i);
						triangleIndices.push_back(offset + i + 1);
						triangleIndices.push_back(offset + i + 2);
					}
				}
			}
			break;
		case enigma_user::pr_trianglefan:
			triangleIndexedVertices.insert(triangleIndexedVertices.end(), vertices.begin(), vertices.end());
			if (indices.size() > 0) {
				for (std::vector<GLuint>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += triangleIndexedCount; }
				for (unsigned i = 1; i < indices.size() - 1; i++) {
					// check for and continue if indexed triangle is degenerate, because the GPU won't render it anyway
					if (indices[0] == indices[i] || indices[0] == indices[i + 1]  || indices[i] == indices[i + 1] ) { continue; }
					triangleIndices.push_back(indices[0]);
					triangleIndices.push_back(indices[i]);
					triangleIndices.push_back(indices[i + 1]);
				}
			} else {
				unsigned offset = (triangleIndexedVertices.size() - vertices.size()) / stride;
				for (unsigned i = 1; i < vertices.size() / stride - 1; i++) {
					triangleIndices.push_back(offset);
					triangleIndices.push_back(offset + i);
					triangleIndices.push_back(offset + i + 1);
				}
			}
			break;
	}

    // Clean up the temporary vertex and index containers now that they have been batched efficiently
    vertices.clear();
    indices.clear();
  }

  void BufferGenerate()
  {
    vector<VertexElement> vdata;
    vector<GLuint> idata;

    vdata.reserve(triangleVertices.size() + lineVertices.size() + pointVertices.size() + triangleIndexedVertices.size() + lineIndexedVertices.size() + pointIndexedVertices.size());
    idata.reserve(triangleIndices.size() + lineIndices.size() + pointIndices.size());

    unsigned interleave = 0;

    triangleIndexedCount = triangleIndices.size();
    if (triangleIndexedCount > 0) {
      vdata.insert(vdata.end(), triangleIndexedVertices.begin(), triangleIndexedVertices.end());
      idata.insert(idata.end(), triangleIndices.begin(), triangleIndices.end());
      interleave += triangleIndexedVertices.size()/GetStride();
    }

    lineIndexedCount = lineIndices.size();
    if (lineIndexedCount > 0) {
      vdata.insert(vdata.end(), lineIndexedVertices.begin(), lineIndexedVertices.end());
      for (std::vector<GLuint>::iterator it = lineIndices.begin(); it != lineIndices.end(); ++it) { *it += interleave; }
      idata.insert(idata.end(), lineIndices.begin(), lineIndices.end());
      interleave += lineIndexedVertices.size()/GetStride();
    }

    pointIndexedCount = pointIndices.size();
    if (pointIndexedCount > 0) {
      vdata.insert(vdata.end(), pointIndexedVertices.begin(), pointIndexedVertices.end());
      for (std::vector<GLuint>::iterator it = lineIndices.begin(); it != lineIndices.end(); ++it) { *it += interleave; }
      idata.insert(idata.end(), pointIndices.begin(), pointIndices.end());
    }

    if (idata.size() > 0) {
      vboindexed = true;
      indexedoffset += vdata.size();

      if (!ibogenerated) {
        glGenBuffers( 1, &indexBuffer );
        ibogenerated = true;
              ibufferSize = idata.size() * sizeof(GLuint);
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, ibufferSize, &idata[0], vbotype );
      } else {
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer );

        if ((double)(idata.size() * sizeof(GLuint)) / (double)ibufferSize > 0.5 ) {
          glBufferData( GL_ELEMENT_ARRAY_BUFFER, idata.size() * sizeof(GLuint), &idata[0], vbotype );
        } else {
          glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, idata.size() * sizeof(GLuint), &idata[0]);
        }
        ibufferSize = idata.size() * sizeof(GLuint);
      }

      // Unbind the buffer we do not need anymore
      glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
      // Clean up temporary interleaved data
      idata.clear();
    } else {
      vboindexed = false;
    }

    if (triangleCount > 0) {
      vdata.insert(vdata.end(), triangleVertices.begin(), triangleVertices.end());
    }

    if (lineCount > 0) {
      vdata.insert(vdata.end(), lineVertices.begin(), lineVertices.end());
    }

    if (pointCount > 0) {
      vdata.insert(vdata.end(), pointVertices.begin(), pointVertices.end());
    }

    if (!vbogenerated) {
      glGenBuffers( 1, &vertexBuffer );
      vbogenerated = true;
      vbufferSize = vdata.size() * sizeof(gs_scalar);
      glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );
      glBufferData( GL_ARRAY_BUFFER, vbufferSize, &vdata[0], vbotype );
    } else {
      glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );

      if ((double)(vdata.size() * sizeof(gs_scalar)) / (double)vbufferSize > 0.5 ) {
        glBufferData( GL_ARRAY_BUFFER, vdata.size() * sizeof(gs_scalar), &vdata[0], vbotype );
      } else {
        glBufferSubData( GL_ARRAY_BUFFER, 0, vdata.size() * sizeof(gs_scalar), &vdata[0]);
      }
      vbufferSize = vdata.size() * sizeof(gs_scalar);
    }

    // Unbind the buffer we do not need anymore
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    // Clean up temporary interleaved data
    vdata.clear();

    // Clean up the data from RAM it is now safe on VRAM
    ClearData();
  }

  void Draw(int vertex_start = 0, int vertex_count = -1)
  {
    if (!GetStride()) { return; }

    if (!vbogenerated || !vbobuffered) {
	  vbobuffered = true;
      BufferGenerate();
    }

    //If there is nothing to render, then there is no need for all the rest
    if (triangleIndexedCount == 0 && lineIndexedCount == 0 && pointIndexedCount == 0 && triangleCount == 0 && lineCount == 0 && pointCount == 0) return;

    if (enigma::transform_needs_update == true){
        enigma::transformation_update();
    }

    //Send transposed (done by GL because of "true" in the function below) matrices to shader
    glUniformMatrix4fv(enigma::shaderprograms[enigma::bound_shader]->uni_viewMatrix,  1, true, enigma::view_matrix);
    glUniformMatrix4fv(enigma::shaderprograms[enigma::bound_shader]->uni_projectionMatrix,  1, true, enigma::projection_matrix);
    glUniformMatrix4fv(enigma::shaderprograms[enigma::bound_shader]->uni_modelMatrix,  1, true, enigma::model_matrix);
    glUniformMatrix4fv(enigma::shaderprograms[enigma::bound_shader]->uni_mvMatrix,  1, true, enigma::mv_matrix);
    glUniformMatrix4fv(enigma::shaderprograms[enigma::bound_shader]->uni_mvpMatrix,  1, true, enigma::mvp_matrix);
    glUniformMatrix3fv(enigma::shaderprograms[enigma::bound_shader]->uni_normalMatrix,  1, true, enigma::normal_matrix);

    //Bind texture
    glsl_uniformi(enigma::shaderprograms[enigma::bound_shader]->uni_texSampler, 0);

    GLsizei stride = GetStride();

    #define OFFSET( P )  ( ( const GLvoid * ) ( sizeof( gs_scalar ) * ( P         ) ) )
    GLsizei STRIDE = stride * sizeof( gs_scalar );

    // Enable vertex array's for fast vertex processing
    glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );
    if (vboindexed) {
      glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer );
    }

    //glEnableClientState(GL_VERTEX_ARRAY);
    unsigned offset = 0;
    glEnableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_vertex);
    glVertexAttribPointer(enigma::shaderprograms[enigma::bound_shader]->att_vertex, vertexStride, GL_FLOAT, 0, STRIDE, OFFSET(offset));
    //glVertexPointer( vertexStride, GL_FLOAT, STRIDE, OFFSET(offset) ); // Set the vertex pointer to the offset in the buffer
    offset += vertexStride;

    if (useNormals){
      glEnableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_normal);
      glVertexAttribPointer(enigma::shaderprograms[enigma::bound_shader]->att_normal, 3, GL_FLOAT, 0, STRIDE, OFFSET(offset));
      offset += 3;
    }

    glsl_uniformf( enigma::shaderprograms[enigma::bound_shader]->uni_color, (float)enigma::currentcolor[0]/255.0f, (float)enigma::currentcolor[1]/255.0f, (float)enigma::currentcolor[2]/255.0f, (float)enigma::currentcolor[3]/255.0f );

    if (useTextures){
         //This part sucks, but is required because models can be drawn without textures even if coordinates are provided
         //like in the case of d3d_model_block
         // Robert: I had to comment out this check due to the change in sampler management, you will need to check all 8 sampler stages if you want to reimplement this check
         // because this model class handles multi-texturing.
        //if (oglmgr->GetBoundTexture() != 0){
            glEnableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_texture);
            glVertexAttribPointer(enigma::shaderprograms[enigma::bound_shader]->att_texture, 2, GL_FLOAT, 0, STRIDE, OFFSET(offset));
            glsl_uniformi(enigma::shaderprograms[enigma::bound_shader]->uni_textureEnable, 1);
        //} else {
          //  glsl_uniformi(enigma::shaderprograms[enigma::bound_shader]->uni_textureEnable, 0);
        //}
      offset += 2;
    } else {
          glsl_uniformi(enigma::shaderprograms[enigma::bound_shader]->uni_textureEnable, 0);
    }

    if (useColors) {
      glsl_uniformi(enigma::shaderprograms[enigma::bound_shader]->uni_colorEnable,1);
    } else {
      glsl_uniformi(enigma::shaderprograms[enigma::bound_shader]->uni_colorEnable,0);
    }
    glEnableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_color);
    glVertexAttribPointer(enigma::shaderprograms[enigma::bound_shader]->att_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, STRIDE, OFFSET(offset)); //Normalization needs to be true, because we pack them as unsigned bytes

    #define OFFSETE( P )  ( ( const GLvoid * ) ( sizeof( GLuint ) * ( P         ) ) )
    offset = vertex_start;

    // Draw the indexed primitives
    if (triangleIndexedCount > 0) {
      glDrawElements(GL_TRIANGLES, (vertex_count==-1?triangleIndexedCount:vertex_count), GL_UNSIGNED_INT, OFFSETE(offset));
      offset += triangleIndexedCount;
    }
    if (lineIndexedCount > 0) {
      glDrawElements(GL_LINES, lineIndexedCount, GL_UNSIGNED_INT, OFFSETE(offset));
      offset += lineIndexedCount;
    }
    if (pointIndexedCount > 0) {
      glDrawElements(GL_POINTS, pointIndexedCount, GL_UNSIGNED_INT, OFFSETE(offset));
    }

    offset = indexedoffset/stride;

    // Draw the unindexed primitives
    if (triangleCount > 0) {
      glDrawArrays(GL_TRIANGLES, (vertex_start==0?offset:vertex_start), (vertex_count==-1?triangleCount:vertex_count));
      offset += triangleCount;
    }
    if (lineCount > 0) {
      glDrawArrays(GL_LINES, offset, lineCount);
      offset += lineCount;
    }
    if (pointCount > 0) {
      glDrawArrays(GL_POINTS, offset, pointCount);
    }

    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    if (vboindexed) {
      glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
    }

    glDisableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_vertex);
    //glDisableClientState(GL_VERTEX_ARRAY);
    if (useTextures) glDisableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_texture); //glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (useNormals) glDisableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_normal); //glDisableClientState(GL_NORMAL_ARRAY);
    if (useColors) glDisableVertexAttribArray(enigma::shaderprograms[enigma::bound_shader]->att_color); //glDisableClientState(GL_COLOR_ARRAY);
  }
};

extern vector<Mesh*> meshes;
