#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "Renderer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"

//Error Handling 코드는 Renderer로 이동

struct ShaderProgramSource
{
	std::string VertexSource;
	std::string FragSource;
};

//셰이더 파일 파싱 함수
static ShaderProgramSource ParseShader(const std::string& filepath)
{
	std::ifstream stream(filepath);

	enum class ShaderType
	{
		NONE = -1, VERTEX = 0, FRAGMENT = 1
	};
	
	std::string line;
	std::stringstream ss[2];
	ShaderType type = ShaderType::NONE;
	while (getline(stream, line))
	{
		if (line.find("#shader") != std::string::npos)
		{
			if (line.find("vertex") != std::string::npos) //vertex 셰이더 섹션
			{
				type = ShaderType::VERTEX;
			}
			else if (line.find("fragment") != std::string::npos) //fragment 셰이더 섹션
			{
				type = ShaderType::FRAGMENT;
			}
		}
		else
		{
			ss[(int)type] << line << '\n'; //코드를 stringstream에 삽입
		}
	}

	return { ss[0].str(), ss[1].str() };
}

static unsigned int CompileShader(unsigned int type, const std::string& source)
{
	unsigned int id = glCreateShader(type); //셰이더 객체 생성(마찬가지)
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr); // 셰이더의 소스 코드 명시
	glCompileShader(id); // id에 해당하는 셰이더 컴파일
	
	// Error Handling(없으면 셰이더 프로그래밍할때 괴롭다...)
	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result); //셰이더 프로그램으로부터 컴파일 결과(log)를 얻어옴
	if (result == GL_FALSE) //컴파일에 실패한 경우
	{
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length); //log의 길이를 얻어옴
		char* message = (char*)alloca(length * sizeof(char)); //stack에 동적할당
		glGetShaderInfoLog(id, length, &length, message); //길이만큼 log를 얻어옴
		std::cout << "셰이더 컴파일 실패! " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << std::endl;
		std::cout << message << std::endl;
		glDeleteShader(id); //컴파일 실패한 경우 셰이더 삭제
		return 0;
	}

	return id;
}

//--------Shader 프로그램 생성, 컴파일, 링크----------//
static unsigned int CreateShader(const std::string& vertexShader, const std::string& fragShader)
{
	unsigned int program = glCreateProgram(); //셰이더 프로그램 객체 생성(int에 저장되는 것은 id)
	unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader); 
	unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragShader);

	//컴파일된 셰이더 코드를 program에 추가하고 링크
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glValidateProgram(program);

	//셰이더 프로그램을 생성했으므로 vs, fs 개별 프로그램은 더이상 필요 없음
	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}

int main(void)
{
	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
		return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); //Opengl 3.3 버전 사용
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	glfwSwapInterval(1); //1이면 vsync rate와 같은 속도로 화면 갱신

	// glfwMakeContextCurrent가 호출된 후에 glewInit이 수행되어야 함
	if (glewInit() != GLEW_OK)
	{
		std::cout << "Error\n";
	}

	std::cout << glGetString(GL_VERSION) << std::endl; //내 플랫폼의 GL_Version 출력해보기

	{ //왜 Scope 사용? -> VertexBuffer/IndexBuffer 소멸자에서 Context가 사라진 이후에 delete하려면 glGetError에 계속 오류가 발생...
	  //따라서 glfwTerminate() 호출해서 Context가 사라지기 전에 소멸자가 호출되도록 scope를 만듬(동적 할당을 통해 매뉴얼하게 소멸시켜도 됨)
		
		float positions[] = { //사각형을 그리기 위해 2차 수정
			-0.5f, -0.5f, //0
			 0.5f, -0.5f, //1
			 0.5f,  0.5f, //2
			-0.5f,  0.5f, //3
		};

		unsigned int indices[] = { //index buffer를 함께 사용(index는 unsigned 타입임에 유의)
			0, 1, 2, //vertex 012로 이루어진 삼각형
			2, 3, 0  //vertex 230로 이루어진 삼각형
		};

		//vao 생성 VertexArray가 담당
		VertexArray va; 
		VertexBuffer vb{ positions, 4 * 2 * sizeof(float) }; 
		VertexBufferLayout layout;
		layout.Push<float>(2); //vertex당 2개의 위치를 표현하는 float 데이터
		//layout.Push<float>(3); //만일 vertex당 색상을 표현하는 3개의 rgb데이터가 더 있었으면 왼쪽과 같이 추가하면, layout에서 알아서 stride 계산
		va.AddBuffer(vb, layout);

		IndexBuffer ib{ indices, 6 };


		//---------Shader 생성---------------//
		ShaderProgramSource source = ParseShader("res/shaders/Basic.shader"); //셰이더 코드 파싱

		unsigned int shader = CreateShader(source.VertexSource, source.FragSource);
		glUseProgram(shader); //BindBuffer와 마찬가지로, 현재 셰이더 프로그램을 "작업 상태"로 놓음
							  //draw call은 작업 상태인 셰이더 프로그램을 사용하여 작업 상태인 버퍼 데이터를 그림

		//--------Uniform 데이터 전달--------//
		// *주의* 이 부분도 당연히 shader가 바인딩("작업 상태")된 상태에서 수행해야 함
		GLCall(int location = glGetUniformLocation(shader, "u_Color")); //셰이더 프로그램에 uniform 데이터를 전달하기 위해서는 uniform의 위치 정보가 필요. 
																//셰이더 객체와 변수 이름을 인자로 넘겨주어 얻어올 수 있음
		ASSERT(location != 1);
		GLCall(glUniform4f(location, 0.2f, 0.3f, 0.8f, 1.0f));

		glBindVertexArray(0); //vao unbind
		glBindBuffer(GL_ARRAY_BUFFER, 0); //객체 인자를 0으로 바인딩하면, 현재 작업 상태를 해제한다는 의미(unbind)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //객체 인자를 0으로 바인딩하면, 현재 작업 상태를 해제한다는 의미(unbind)
		glUseProgram(0); //객체 인자를 0으로 바인딩하면, 현재 작업 상태를 해제한다는 의미(unbind)


		float r = 0.0f;
		float increment = 0.05f;
		/* Loop until the user closes the window */
		while (!glfwWindowShouldClose(window))
		{
			/* Render here */
			glClear(GL_COLOR_BUFFER_BIT);

			GLCall(glUseProgram(shader));
			GLCall(glUniform4f(location, r, 0.3f, 0.8f, 1.0f));

			va.Bind();
			ib.Bind(); //한 모델이 다른 Material을 사용할 경우 여러 Draw call에 걸쳐 그려지는 경우가 많고, 이러한 경우 index buffer로 모델의 부분을 구분하는 경우가 많음

			GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr)); //Draw call

			if (r > 1.0f)
				increment = -0.05f;
			if (r < 0.0f)
				increment = 0.05f;

			r += increment;

			/* Swap front and back buffers */
			glfwSwapBuffers(window);

			/* Poll for and process events */
			glfwPollEvents();
		}

		glDeleteProgram(shader); //셰이더 삭제
	}
	

	glfwTerminate();
	return 0;
}