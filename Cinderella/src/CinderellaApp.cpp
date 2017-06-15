#include "Resources.h"

#include "cinder/ObjLoader.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Arcball.h"
#include "cinder/CameraUi.h"
#include "cinder/Sphere.h"
#include "cinder/ImageIo.h"
#include "cinder/ip/Checkerboard.h"

#include "Resources.h"

using namespace ci;
using namespace ci::app;

class CinderellaApp : public App {
  public:
    void setup() override;
    void draw() override;

  private:
    void mouseDown(MouseEvent event) override;
    void mouseDrag(MouseEvent event) override;
    void keyDown(KeyEvent event) override;
    void loadObj(const DataSourceRef &dataSource);
    void frameCurrentObject();

    Arcball arcball_;
    CameraUi camUi_;
    CameraPersp camera_;
    TriMeshRef mesh_;
    Sphere bounding_sphere_;
    gl::BatchRef batch_, skybox_batch_;
    gl::GlslProgRef pbrShader_, skyBoxShader_;
    gl::TextureCubeMapRef irradianceMap_, radianceMap_;
    Color base_color_;
    float gamma_, exposure_, specular_, roughness_, metallic_;
};

void CinderellaApp::setup() {
    //setup shaders
    pbrShader_ = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("pbr.vert")).fragment(loadAsset("pbr.frag")));
    skyBoxShader_ = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("skybox.vert")).fragment(loadAsset("skybox.frag")));

    //create camera and camera ui
    camera_ = CameraPersp(getWindowWidth(), getWindowHeight(), 50.0f, 1.0f, 1000.0f).calcFraming(Sphere(vec3(0.0f), 12.0f));
    camUi_ = CameraUi(&camera_, getWindow(), -1);

    //create skybox with shader
    skybox_batch_ = gl::Batch::create(geom::Cube().size(vec3(500)), skyBoxShader_);

    //load model
    console() << "Loading object model from file" << std::endl;
    loadObj(loadAsset("dragon.obj"));

    //load cubemap
    auto cubeMapFormat = gl::TextureCubeMap::Format().mipmap().internalFormat(GL_RGB16F).minFilter(GL_LINEAR_MIPMAP_LINEAR).magFilter(GL_LINEAR);
    irradianceMap_ = gl::TextureCubeMap::createFromDds(loadAsset("irradiancemap.dds"), cubeMapFormat);
    radianceMap_ = gl::TextureCubeMap::createFromDds(loadAsset("radiancemap.dds"), cubeMapFormat);

    arcball_ = Arcball(&camera_, bounding_sphere_);

    //set colors etc
    base_color_ = Color::hex(0xFF0000);
    specular_ = 1.0f;
    gamma_ = 2.2f;
    exposure_ = 5.5f;
    roughness_ = 0.4f;
    metallic_ = 0.1f;

    getSignalUpdate().connect([this]() {
        getWindow()->setTitle(std::to_string((int)getAverageFps()) + " fps");
    });
}

void CinderellaApp::mouseDown(MouseEvent event) {
    if (event.isMetaDown())
        camUi_.mouseDown(event);
    else
        arcball_
            .mouseDown(event);
}

void CinderellaApp::mouseDrag(MouseEvent event) {
    if (event.isMetaDown())
        camUi_.mouseDrag(event);
    else
        arcball_
            .mouseDrag(event);
}

void CinderellaApp::loadObj(const DataSourceRef &dataSource) {
    ObjLoader loader(dataSource);
    mesh_ = TriMesh::create(loader);

    if (!loader.getAvailableAttribs().count(geom::NORMAL))
        mesh_->recalculateNormals();

    batch_ = gl::Batch::create(*mesh_, pbrShader_);

    bounding_sphere_ = Sphere::calculateBoundingSphere(mesh_->getPositions<3>(), mesh_->getNumVertices());
    arcball_.setSphere(bounding_sphere_);
}

void CinderellaApp::frameCurrentObject() {
    camera_ = camera_.calcFraming(bounding_sphere_);
}

void CinderellaApp::keyDown(KeyEvent event) {
    if (event.getChar() == 'l') {
        fs::path path = getOpenFilePath();
        if (!path.empty()) {
            loadObj(loadFile(path));
        }
    }
    else if (event.getChar() == 'f') {
        frameCurrentObject();
    }
}

void CinderellaApp::draw() {
    gl::enableDepthWrite();
    gl::enableDepthRead();
    gl::clear(Color(0, 0, 0));
    gl::ScopedDepth(true);

    //bind cubemaps
    gl::ScopedTextureBind scopedTexBind0(radianceMap_, 0);
    gl::ScopedTextureBind scopedTexBind1(irradianceMap_, 1);

    auto shader = batch_->getGlslProg();
    shader->uniform("radianceMap", 0);
    shader->uniform("irradianceMap", 1);

    shader->uniform("baseColor", base_color_);
    shader->uniform("specular", specular_);

    shader->uniform("exposure", exposure_);
    shader->uniform("gamma", gamma_);
    shader->uniform("roughness", roughness_);
    shader->uniform("metallic", metallic_);

    gl::setMatrices(camera_);
    gl::pushMatrices();
    gl::rotate(arcball_.getQuat());
    batch_->draw();

    //skybox
    shader = skybox_batch_->getGlslProg();
    shader->uniform("exposure", exposure_);
    shader->uniform("gamma", gamma_);
    skybox_batch_->draw();

    gl::popMatrices();
}
CINDER_APP(CinderellaApp, RendererGl)
