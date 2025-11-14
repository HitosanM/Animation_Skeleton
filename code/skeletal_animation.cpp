#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>



#include <iostream>


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// animations accessible from input handling
Animation* g_anim1 = nullptr;
Animation* g_anim2 = nullptr;
Animation* g_currentAnimation = nullptr;
Animator* g_animator = nullptr;

// whether to draw the ship
bool g_drawShip = false;

// bone to attach ship to (change to match your model's bone name)
const std::string attachBoneName = "mixamorig_RightHandThumb3"; // attach to thumb tip

// ship attachment state
bool g_shipAttached = false;
// countdown timer for attachment
float g_attachmentTimer = 0.0f;
// bone index to attach to (determined at startup)
int g_attachBoneIndex = 29;

int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
	stbi_set_flip_vertically_on_load(true);

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);

	// build and compile shaders (use FileSystem::getPath so files are found at runtime)
	// -----------------------------------------------------------------------------
	Shader ourShader(FileSystem::getPath("src/8.guest/2020/skeletal_animation/anim_model.vs").c_str(),
                      FileSystem::getPath("src/8.guest/2020/skeletal_animation/anim_model.fs").c_str());
	Shader staticShader(FileSystem::getPath("src/8.guest/2020/skeletal_animation/model.vs").c_str(),
                        FileSystem::getPath("src/8.guest/2020/skeletal_animation/model.fs").c_str());
	
	// load models
	// -----------
	Model ourModel(FileSystem::getPath("resources/objects/w/w.dae"));
	Model ShipModel(FileSystem::getPath("resources/objects/3d_daily_ghibli/Untitled.dae"));
	// create two animations from different files
	g_anim1 = new Animation(FileSystem::getPath("resources/objects/w/w.dae"), &ourModel);
	g_anim2 = new Animation(FileSystem::getPath("resources/objects/PickingUp/P.dae"), &ourModel);
	// animator starts with first animation
	g_currentAnimation = g_anim1;
	g_animator = new Animator(g_currentAnimation);

    // determine bone index for attachment (if present)
    {
        auto boneMap = g_anim1->GetBoneIDMap(); // bones come from model; either animation should have same bones
        auto it = boneMap.find(attachBoneName);
        if (it != boneMap.end()) {
            g_attachBoneIndex = it->second.id;
            std::cout << "Attach bone '" << attachBoneName << "' found with index " << g_attachBoneIndex << std::endl;
        } else {
            std::cout << "Attach bone '" << attachBoneName << "' not found. Attachment will use default transform." << std::endl;
            g_attachBoneIndex = -1;
        }
    }

	std::cout << "ShipModel mesh count: " << ShipModel.meshes.size() << std::endl;
	if (ShipModel.meshes.size() == 0) {
		std::cerr << "Warning: ShipModel loaded with zero meshes. Check path / file." << std::endl;
	}

	// small offset applied when attaching to hand so ship sits nicely in hand
	glm::mat4 shipAttachOffset = glm::mat4(1.0f);
	// tune these values until the ship sits correctly in the hand
	shipAttachOffset = glm::translate(shipAttachOffset, glm::vec3(0.0f, -0.05f, 0.05f));
	// rotate to orient ship to hand (adjust axes as needed)
	shipAttachOffset = glm::rotate(shipAttachOffset, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	// uniform scale to fit hand
	shipAttachOffset = glm::scale(shipAttachOffset, glm::vec3(0.5f));

	// draw in wireframe
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);
		g_animator->UpdateAnimation(deltaTime);
		
		// render
		// ------
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// don't forget to enable shader before setting uniforms
		ourShader.use();

		// view/projection transformations
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		ourShader.setMat4("projection", projection);
		ourShader.setMat4("view", view);

        auto transforms = g_animator->GetFinalBoneMatrices();
		for (int i = 0; i < transforms.size(); ++i)
			ourShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);


		// render the animated model
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, -0.4f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::scale(model, glm::vec3(.5f, .5f, .5f));
	
		ourShader.setMat4("model", model);
		ourModel.Draw(ourShader);

		// draw ship: place it in front of ourModel by default
		// compute ship local position relative to model, with optional orientation and scale
		glm::vec3 shipLocalPos = glm::vec3(0.0f, 0.125f, 0.08f); // in model's local space: x right, y up, z forward(-)
		glm::mat4 shipModelMat = model
			* glm::translate(glm::mat4(1.0f), shipLocalPos)
			
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.005f)); // adjust size to 10.0f per request
		if (g_drawShip) {
			// debug: print ship world position and approximate scale on first draw
			static bool printedTransform = false;
			if (!printedTransform) {
				glm::vec3 shipWorldPos = glm::vec3(shipModelMat[3]);
				float approxScale = glm::length(glm::vec3(shipModelMat[0]));
				std::cout << "Ship world pos: (" << shipWorldPos.x << ", " << shipWorldPos.y << ", " << shipWorldPos.z << ") scale=" << approxScale << std::endl;
				printedTransform = true;
			}

			// draw ship normally (filled)
			if (g_shipAttached) {
				// attach ship to the bone: compute bone global transform from finalBoneMatrix and stored offset
				if (g_attachBoneIndex >= 0 && g_attachBoneIndex < (int)transforms.size()) {
					auto boneMap = g_currentAnimation->GetBoneIDMap();
					auto it = boneMap.find(attachBoneName);
					if (it != boneMap.end()) {
						glm::mat4 finalBoneMat = transforms[g_attachBoneIndex];
						glm::mat4 offset = it->second.offset;
						// finalBoneMat = globalTransform * offset    =>    globalTransform = finalBoneMat * inverse(offset)
						glm::mat4 boneGlobal = finalBoneMat * glm::inverse(offset);
						// ship world transform = model * boneGlobal * shipAttachOffset
						shipModelMat = model * boneGlobal * shipAttachOffset;
					} else {
						// fallback: use final matrix directly
						shipModelMat = transforms[g_attachBoneIndex] * shipAttachOffset;
					}
				} else {
					// invalid index - do nothing (keep default shipModelMat)
				}
			}

			// Use the computed shipModelMat
			staticShader.use();
			staticShader.setMat4("projection", projection);
			staticShader.setMat4("view", view);
			staticShader.setMat4("model", shipModelMat);
			ShipModel.Draw(staticShader);
		}


		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// cleanup
	delete g_anim1;
	delete g_anim2;
	delete g_animator;

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();
	return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
	static bool key1Pressed = false;
	static bool key2Pressed = false;

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);

	// switch animations with 1 and 2 (edge-detected)
	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
		if (!key1Pressed && g_animator && g_anim1) {
			g_animator->PlayAnimation(g_anim1);
			g_currentAnimation = g_anim1;
			g_drawShip = false; // hide ship when pressing 1
			g_shipAttached = false; // reset attachment state
		}
		key1Pressed = true;
	} else {
		key1Pressed = false;
	}

	if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
		if (!key2Pressed && g_animator && g_anim2) {
			g_animator->PlayAnimation(g_anim2);
			g_currentAnimation = g_anim2;
			g_drawShip = true; // show ship when pressing 2
			g_shipAttached = false; // reset attachment state

			// start countdown timer for attachment
			g_attachmentTimer = 2.5f; // 3 seconds
			std::cout << "Attachment countdown started. Ship will attach in " << g_attachmentTimer << " seconds." << std::endl;
		}
		key2Pressed = true;
	} else {
		key2Pressed = false;
	}

	// update attachment state based on timer
	if (g_attachmentTimer > 0.0f) {
		g_attachmentTimer -= deltaTime;
		if (g_attachmentTimer <= 0.0f) {
			g_shipAttached = true; // attach ship to hand
			std::cout << "Attachment occurred. Ship is now attached." << std::endl;
		}
	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}
