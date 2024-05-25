#include "Animation.h"
#include "Application.h"
#include "AnimationBuilder.h"

void Animation::update()
{
    if (!m_active) {
        //fmt::print("Animation is not active.\n");
        return;
    }

    m_elapsedTime += Application::s_deltaTime;
    if (m_elapsedTime > m_duration) {
        m_active = false;
        m_elapsedTime = m_duration;
    }

    float t = m_elapsedTime / m_duration;
    m_updateFunc(t);
    //fmt::print("Animation updated. Elapsed time: {}, t: {}\n", m_elapsedTime, t);
}

void Animation::to_json(nlohmann::json& j) const {
    nlohmann::json data_json;
    if (m_data) {
        try {
            m_data->to_json(data_json);
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to serialize animation data. Exception: {}\n", e.what());
            data_json = nullptr;
        }
    }
    else {
        data_json = nullptr;
    }

    j = nlohmann::json{
        {"duration", m_duration},
        {"active", m_active},
        {"elapsedTime", m_elapsedTime},
        {"data", data_json}
    };
}

void Animation::from_json(const nlohmann::json& j) {
    try {
        m_duration = j.at("duration").get<float>();
        m_active = j.at("active").get<bool>();
        m_elapsedTime = j.at("elapsedTime").get<float>();

        if (j.contains("data") && !j["data"].is_null()) {
            std::string type = j["data"]["type"];
            if (type == "move") {
                m_data = std::make_shared<MoveAnimationData>();
            }
            else if (type == "rotate") {
                m_data = std::make_shared<RotateAnimationData>();
            }
            else if (type == "orbit") {
                m_data = std::make_shared<OrbitAnimationData>();
            }
            m_data->from_json(j.at("data"));
        }
        fmt::print("Animation initialized successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to deserialize animation. Exception: {}\n", e.what());
        throw; // Rzuæmy wyj¹tek dalej, aby ³atwiej by³o œledziæ b³¹d
    }
}


