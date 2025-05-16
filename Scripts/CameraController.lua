-- CameraController.lua
-- A camera control script for entities with TransformComponent

-- Camera control parameters
local DEFAULT_CONFIG = {
    moveSpeed = 10.0,
    mouseSensitivity = 0.1,
    minPitchAngle = -89.0,
    maxPitchAngle = 89.0,
    initialYaw = 0.0,
    initialPitch = 0.0,
}

-- Stan wewnętrzny kamery
local movementState = {
    forward = false,
    backward = false,
    left = false,
    right = false,
    up = false,
    down = false
}

-- Helper function to clamp values
local function clamp(value, min, max)
    return math.max(min, math.min(max, value))
end

-- Oblicz wektory orientacji kamery
local function updateCameraVectors(self)
    local yawRad = math.rad(self.yaw)
    local pitchRad = math.rad(self.pitch)

    -- Oblicz wektor forward zgodnie z implementacją C++
    local forward = {
        x = -math.cos(pitchRad) * math.sin(yawRad),
        y = math.sin(pitchRad),
        z = -math.cos(pitchRad) * math.cos(yawRad)
    }
    
    -- Normalizacja wektora
    local length = math.sqrt(forward.x^2 + forward.y^2 + forward.z^2)
    forward.x = forward.x / length
    forward.y = forward.y / length
    forward.z = forward.z / length

    -- Oblicz wektory right i up
    local worldUp = Vector3(0, 1, 0)
    local right = Vector3(-forward.z, 0, forward.x):normalize()
    local up = right:cross(Vector3(forward.x, forward.y, forward.z)):normalize()

    self.forwardVector = Vector3(forward.x, forward.y, forward.z)
    self.rightVector = right
    self.upVector = up

    if self.transform then
        self.transform:setRotation(Vector3(pitchRad * (180/math.pi), yawRad * (180/math.pi), 0.0))
    end
end

-- Obsługa wejścia z klawiatury
local function processKeyboardInput(self, deltaTime)
    local velocity = Vector3(0.0, 0.0, 0.0)
    
    if movementState.forward  then velocity = velocity + self.forwardVector end
    if movementState.backward then velocity = velocity - self.forwardVector end
    if movementState.right    then velocity = velocity + self.rightVector end
    if movementState.left     then velocity = velocity - self.rightVector end
    if movementState.up       then velocity = velocity + Vector3(0, 1, 0) end
    if movementState.down     then velocity = velocity - Vector3(0, 1, 0) end

    if velocity:length() > 0 then
        velocity = velocity:normalize()
        velocity = velocity * self.config.moveSpeed * deltaTime
        self.transform:setPosition(self.transform:getPosition() + velocity)
    end
end

-- Obsługa ruchu myszą
local function processMouseInput(self, deltaTime)
    if Input.isMouseButtonHeld(MouseButton.Right) then
        local mouseDelta = Input.getMouseDelta()
        if mouseDelta.x ~= 0 or mouseDelta.y ~= 0 then
            -- Aktualizacja rotacji
            self.yaw = (self.yaw - mouseDelta.x * self.config.mouseSensitivity) % 360.0
            self.pitch = clamp(self.pitch - mouseDelta.y * self.config.mouseSensitivity, 
                             self.config.minPitchAngle, 
                             self.config.maxPitchAngle)
            
            updateCameraVectors(self)
        end
    end
end

-- Obsługa scrolla myszy
local function processMouseScroll(self)
    local scroll = Input.getMouseScrollDelta()
    if scroll ~= 0 then
        self.config.moveSpeed = clamp(self.config.moveSpeed + scroll * 0.5, 1.0, 20.0)
    end
end

-- Funkcje callback
function OnCreate(self)
    self.config = {}
    for k, v in pairs(DEFAULT_CONFIG) do
        self.config[k] = v
    end

    self.yaw = self.config.initialYaw
    self.pitch = self.config.initialPitch
    
    if not registry:hasComponent(self.entity, "Transform") then
        Logger.error("CameraController requires TransformComponent")
        return
    end
    
    self.transform = registry:getComponent(self.entity, "Transform")
    updateCameraVectors(self)
end

function OnUpdate(self, deltaTime)
    -- Obsługa klawiszy
    movementState.forward  = Input.isKeyHeld(Input.KEY_W)
    movementState.backward = Input.isKeyHeld(Input.KEY_S)
    movementState.left     = Input.isKeyHeld(Input.KEY_A)
    movementState.right    = Input.isKeyHeld(Input.KEY_D)
    movementState.up       = Input.isKeyHeld(Input.KEY_SPACE)
    movementState.down     = Input.isKeyHeld(Input.KEY_LEFT_CONTROL) or Input.isKeyHeld(Input.KEY_RIGHT_CONTROL)

    -- Aktualizacja kursora
    if Input.isMouseButtonHeld(MouseButton.Right) then
        Input.setCursorMode(CursorMode.Disabled)
    else
        Input.setCursorMode(CursorMode.Normal)
    end

    processMouseInput(self, deltaTime)
    processMouseScroll(self)
    processKeyboardInput(self, deltaTime)
end

function OnDestroy(self)
    Input.setCursorMode(CursorMode.Normal)
end