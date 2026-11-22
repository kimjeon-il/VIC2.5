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
// Fixed-size, test-only +Z palette-ID window, not a campaign texture API.
struct ReviewTile {
    static constexpr unsigned side=128;
    double u_min{},u_max{},v_min{},v_max{};
    std::vector<std::uint32_t> ids;
};
class RasterGlobe {
    GLFWwindow* window_{};
    std::uint32_t synthetic_gpu_index_{};
    GLuint program_{},vao_{},vbo_{},raster_id_{},palette_{};
    GLsizei vertices_{};
    bool initialized_glfw_{};
    bool review_{};
    float center_x_{},center_y_{},scale_x_{.80f},scale_y_{.80f};
    static constexpr const char* vertex_source=R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in float face;
out vec2 texcoord;
flat out int face_index;
uniform vec4 clip_transform;
void main() {
    texcoord=uv;
    face_index=int(face+.5);
    gl_Position=vec4((position.xy-clip_transform.xy)*clip_transform.zw,-position.z*.30,1.0);
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
    static std::vector<Vertex> build_mesh(const ReviewTile* review) {
        std::vector<Vertex> verts;verts.reserve(6*12*12*6);
        constexpr int grid=12;
        for(std::uint8_t face=review?4:0;face<(review?5:6);++face) {
            const double u=review?review->u_min:-1,v=review?review->v_min:-1;
            const double uw=review?review->u_max-u:2,vh=review?review->v_max-v:2;
            const auto point=[&](int x,int y) {
                const double s=static_cast<double>(x)/grid,t=static_cast<double>(y)/grid;
                auto item=vertex(face,u+uw*s,v+vh*t);
                item.u=static_cast<float>(s);item.v=static_cast<float>(t);
                return item;
            };
            for(int y=0;y<grid;++y)
                for(int x=0;x<grid;++x) {
                    const auto p=point(x,y),q=point(x+1,y),r=point(x+1,y+1),t=point(x,y+1);
                    verts.insert(verts.end(),{p,q,r,p,r,t});
                }
        }
        return verts;
    }
public:
    RasterGlobe()=default;
    RasterGlobe(const RasterGlobe&)=delete;
    RasterGlobe& operator=(const RasterGlobe&)=delete;
    ~RasterGlobe(){destroy();}
    static std::array<unsigned char,4> review_color(unsigned id) {
        switch(id) {
          case 0:return {50,55,70,255}; // Outside the study cut, not framebuffer clear.
          case 1:return {224,32,32,255};
          case 2:return {32,224,32,255};
          case 3:return {224,192,32,255};
          case 101:return {32,96,224,255};
          default:return {0,0,0,0};
        }
    }
    void initialize(std::uint32_t diagnostic_gpu_index=0,const ReviewTile* review=nullptr) {
        require_gl(!window_&&diagnostic_gpu_index<65536u,
                   "GL context already active or dense GPU ID out of palette");
        require_gl(!review||(!diagnostic_gpu_index&&
            review->u_min>=-1&&review->u_min<review->u_max&&review->u_max<=1&&
            review->v_min>=-1&&review->v_min<review->v_max&&review->v_max<=1&&
            review->ids.size()==ReviewTile::side*ReviewTile::side&&
            std::all_of(review->ids.begin(),review->ids.end(),[](auto id){return id<=3||id==101;})),
            "invalid +Z review window");
        synthetic_gpu_index_=diagnostic_gpu_index;
        review_=review!=nullptr;
        center_x_=center_y_=0;
        scale_x_=scale_y_=.80f;
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
        const auto vertices=build_mesh(review);
        if(review) {
            const auto x=std::minmax_element(vertices.begin(),vertices.end(),
                [](const auto& a,const auto& b){return a.x<b.x;});
            const auto y=std::minmax_element(vertices.begin(),vertices.end(),
                [](const auto& a,const auto& b){return a.y<b.y;});
            require_gl(x.second->x-x.first->x>1e-6f&&y.second->y-y.first->y>1e-6f,
                "collapsed orthographic review window");
            center_x_=(x.first->x+x.second->x)*.5f;
            center_y_=(y.first->y+y.second->y)*.5f;
            scale_x_=1.8f/(x.second->x-x.first->x);
            scale_y_=1.8f/(y.second->y-y.first->y);
        }
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
        const auto side=review?ReviewTile::side:8u;
        const auto layer=side*side;
        std::vector<std::uint32_t> ids(layer*6);
        if(review)std::copy(review->ids.begin(),review->ids.end(),ids.begin()+4*layer);
        else for(std::uint32_t face=0;face<6;++face)
            std::fill(ids.begin()+face*layer,ids.begin()+(face+1)*layer,
                      synthetic_gpu_index_?synthetic_gpu_index_:face+1);
        glGenTextures(1,&raster_id_);
        glBindTexture(GL_TEXTURE_2D_ARRAY,raster_id_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_R32UI,side,side,6,0,GL_RED_INTEGER,
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
    std::array<unsigned char,4> review_pixel(raster::Vector3 direction) {
        require_gl(review_&&raster::direction_to_cube(direction).face==4,
            "sample outside +Z review face");
        const auto x=static_cast<int>(64*(1+(direction.x-center_x_)*scale_x_));
        const auto y=static_cast<int>(64*(1+(direction.y-center_y_)*scale_y_));
        return render(0,x,y);
    }
    std::array<unsigned char,4> render(int map_mode,int x=64,int y=64) {
        require_gl(window_&&map_mode>=0&&map_mode<=2&&x>=0&&x<128&&y>=0&&y<128,"invalid renderer or map mode");
        glfwMakeContextCurrent(window_);
        std::vector<unsigned char> colors(256*256*4,0);
        for(int i=1;i<=6;++i) {
            const auto at=4*i;
            colors[at+0]=map_mode==0?220:20;
            colors[at+1]=map_mode==1?220:20;
            colors[at+2]=map_mode==2?220:20;
            colors[at+3]=255;
        }
        if(review_)for(unsigned id:{0u,1u,2u,3u,101u}) {
            const auto color=review_color(id);
            std::copy(color.begin(),color.end(),colors.begin()+4*id);
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
        glUniform4f(glGetUniformLocation(program_,"clip_transform"),
                    center_x_,center_y_,scale_x_,scale_y_);
        glUniform1i(glGetUniformLocation(program_,"province_id"),0);
        glUniform1i(glGetUniformLocation(program_,"political_colors"),1);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES,0,vertices_);
        glFinish();
        std::array<unsigned char,4> pixel{};
        glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
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
