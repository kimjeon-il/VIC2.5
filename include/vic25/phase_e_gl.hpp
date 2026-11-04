#pragma once
// Optional OpenGL 3.3 implementation for an actual GPU (including Mesa software
// GPU in CI). Static integer province index tiles and a mutable political LUT
// are uploaded independently. No actual worldwide GIS dataset is included.
#include "vic25/phase_e_raster.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace vic25::phasee::gpu {
inline void require_gl(bool condition,const char* why) {
    if(!condition)throw std::runtime_error(why);
}
inline GLuint shader(GLenum kind,const char* source) {
    const GLuint obj=glCreateShader(kind);
    glShaderSource(obj,1,&source,nullptr);
    glCompileShader(obj);
    GLint ok=0;glGetShaderiv(obj,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        char log[2048]={};
        glGetShaderInfoLog(obj,static_cast<GLsizei>(sizeof(log)),nullptr,log);
        glDeleteShader(obj);
        throw std::runtime_error(std::string("GLSL compile: ")+log);
    }
    return obj;
}
struct Vertex {
    float x,y,z,u,v,face;
};
class RasterGlobe {
    GLFWwindow* window_{};
    std::uint32_t synthetic_gpu_index_{};
    GLuint program_{},vao_{},vbo_{},raster_id_{},palette_{};
    GLsizei vertices_{};
    bool initialized_glfw_{};
    static constexpr const char* vertex_source=R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in float face;
out vec2 texcoord;
flat out int face_index;
void main() {
    texcoord=uv;
    face_index=int(face+.5);
    gl_Position=vec4(position.xy*.80,-position.z*.30,1.0);
}
)";
    static constexpr const char* fragment_source=R"(#version 330 core
in vec2 texcoord;
flat in int face_index;
uniform usampler2DArray province_id;
uniform sampler2D political_colors;
out vec4 fragment_color;
void main() {
    ivec2 dimensions=textureSize(province_id,0).xy;
    ivec2 ix=ivec2(clamp(texcoord,vec2(0),vec2(0.99999))*vec2(dimensions));
    uint index=texelFetch(province_id,ivec3(ix,face_index),0).r;
    ivec2 key=ivec2(int(index%256u),int(index/256u));
    fragment_color=texelFetch(political_colors,key,0);
}
)";
    static Vertex vertex(std::uint8_t face,double u,double v) {
        const auto d=raster::cube_to_direction({face,u,v});
        return {static_cast<float>(d.x),static_cast<float>(d.y),
                static_cast<float>(d.z),static_cast<float>((u+1)*.5),
                static_cast<float>((v+1)*.5),static_cast<float>(face)};
    }
    static std::vector<Vertex> build_mesh() {
        std::vector<Vertex> verts;verts.reserve(6*12*12*6);
        constexpr int grid=12;
        for(std::uint8_t face=0;face<6;++face)
            for(int y=0;y<grid;++y)
                for(int x=0;x<grid;++x) {
                    const double a=-1.+2.*x/grid,b=-1.+2.*y/grid;
                    const double c=-1.+2.*(x+1)/grid,d=-1.+2.*(y+1)/grid;
                    const auto p=vertex(face,a,b),q=vertex(face,c,b),
                               r=vertex(face,c,d),s=vertex(face,a,d);
                    verts.insert(verts.end(),{p,q,r,p,r,s});
                }
        return verts;
    }
public:
    RasterGlobe()=default;
    RasterGlobe(const RasterGlobe&)=delete;
    RasterGlobe& operator=(const RasterGlobe&)=delete;
    ~RasterGlobe(){destroy();}
    void initialize(std::uint32_t diagnostic_gpu_index=0) {
        require_gl(!window_&&diagnostic_gpu_index<65536u,
                   "GL context already active or dense GPU ID out of palette");
        synthetic_gpu_index_=diagnostic_gpu_index;
        require_gl(glfwInit()==GLFW_TRUE,"glfwInit failed");
        initialized_glfw_=true;
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
        window_=glfwCreateWindow(128,128,"VIC2.5 raster globe test",nullptr,nullptr);
        require_gl(window_!=nullptr,"OpenGL 3.3 context unavailable");
        glfwMakeContextCurrent(window_);
        glewExperimental=GL_TRUE;
        require_gl(glewInit()==GLEW_OK,"GLEW init");
        while(glGetError()!=GL_NO_ERROR){} // GLEW core-profile startup code
        GLint major=0,minor=0;
        glGetIntegerv(GL_MAJOR_VERSION,&major);
        glGetIntegerv(GL_MINOR_VERSION,&minor);
        require_gl(major>3||(major==3&&minor>=3),"GL3.3 unsupported");
        const auto a=shader(GL_VERTEX_SHADER,vertex_source);
        const auto b=shader(GL_FRAGMENT_SHADER,fragment_source);
        program_=glCreateProgram();
        glAttachShader(program_,a);glAttachShader(program_,b);
        glLinkProgram(program_);
        glDeleteShader(a);glDeleteShader(b);
        GLint linked=0;glGetProgramiv(program_,GL_LINK_STATUS,&linked);
        require_gl(linked!=0,"shader program linking failed");
        const auto vertices=build_mesh();
        vertices_=static_cast<GLsizei>(vertices.size());
        glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);
        glGenBuffers(1,&vbo_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(Vertex)),
                     vertices.data(),GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(0));
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),
                              reinterpret_cast<void*>(3*sizeof(float)));
        glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),
                              reinterpret_cast<void*>(5*sizeof(float)));
        glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glEnableVertexAttribArray(2);
        std::vector<std::uint32_t> ids(8*8*6);
        for(std::uint32_t face=0;face<6;++face)
            std::fill(ids.begin()+face*64,ids.begin()+(face+1)*64,
                      synthetic_gpu_index_?synthetic_gpu_index_:face+1);
        glGenTextures(1,&raster_id_);
        glBindTexture(GL_TEXTURE_2D_ARRAY,raster_id_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_R32UI,8,8,6,0,GL_RED_INTEGER,
                     GL_UNSIGNED_INT,ids.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenTextures(1,&palette_);
        glBindTexture(GL_TEXTURE_2D,palette_);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        std::vector<unsigned char> blank(256*256*4,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,
                     GL_UNSIGNED_BYTE,blank.data());
        require_gl(glGetError()==GL_NO_ERROR,"GPU texture or VBO creation failed");
    }
    std::array<unsigned char,4> render(int map_mode) {
        require_gl(window_&&map_mode>=0&&map_mode<=2,"invalid renderer or map mode");
        glfwMakeContextCurrent(window_);
        std::vector<unsigned char> colors(256*256*4,0);
        for(int i=1;i<=6;++i) {
            const auto at=4*i;
            colors[at+0]=map_mode==0?220:20;
            colors[at+1]=map_mode==1?220:20;
            colors[at+2]=map_mode==2?220:20;
            colors[at+3]=255;
        }
        if(synthetic_gpu_index_){
            const auto at=4u*synthetic_gpu_index_;
            colors[at+0]=map_mode==0?220:20;
            colors[at+1]=map_mode==1?220:20;
            colors[at+2]=map_mode==2?220:20;
            colors[at+3]=255;
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,palette_);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,colors.data());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY,raster_id_);
        glViewport(0,0,128,128);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(program_);
        glUniform1i(glGetUniformLocation(program_,"province_id"),0);
        glUniform1i(glGetUniformLocation(program_,"political_colors"),1);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES,0,vertices_);
        glFinish();
        std::array<unsigned char,4> pixel{};
        glReadPixels(64,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        require_gl(glGetError()==GL_NO_ERROR,"raster globe GPU draw/read failed");
        return pixel;
    }
    void destroy() noexcept {
        if(window_) {
            glfwMakeContextCurrent(window_);
            if(raster_id_)glDeleteTextures(1,&raster_id_);
            if(palette_)glDeleteTextures(1,&palette_);
            if(vbo_)glDeleteBuffers(1,&vbo_);
            if(vao_)glDeleteVertexArrays(1,&vao_);
            if(program_)glDeleteProgram(program_);
            raster_id_=palette_=vbo_=vao_=program_=0;
            glfwDestroyWindow(window_);
            window_=nullptr;
        }
        if(initialized_glfw_) {glfwTerminate();initialized_glfw_=false;}
    }
};
} // namespace vic25::phasee::gpu
