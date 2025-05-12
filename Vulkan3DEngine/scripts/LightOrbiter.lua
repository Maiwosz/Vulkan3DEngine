local function UpdateOrbitPosition(self)
    -- Obliczenia pozycji na okręgu w płaszczyźnie X-Z
    local x = self.orbitCenter.x + self.orbitRadius * math.cos(self.currentAngle)
    local z = self.orbitCenter.z + self.orbitRadius * math.sin(self.currentAngle)
    
    -- Aplikowanie pozycji z ustaloną wysokością Y
    self.transform:setPosition(Vector3(x, self.orbitCenter.y, z))
end

function OnCreate(self)
    -- Konfiguracja orbity
    self.orbitCenter = Vector3(0.0, 4.0, 0.0)  -- Środek orbity (X,Y,Z)
    self.orbitRadius = 12.0     -- Promień okręgu
    self.orbitSpeed = 1.0       -- Prędkość kątowa (rad/s)
    self.initialAngle = 0.0     -- Kąt startowy w stopniach
    
    -- Inicjalizacja zmiennych
    self.currentAngle = math.rad(self.initialAngle)  -- Konwersja na radiany
    
    -- Bezpieczne pobieranie transformacji - użycie nowego API
    if not registry:hasComponent(self.entity, "Transform") then
        Logger.error("Missing TransformComponent for entity {}", self.entity.id)
        return
    end
    self.transform = registry:getComponent(self.entity, "Transform")
    
    -- Początkowa pozycja
    UpdateOrbitPosition(self)
end

function OnUpdate(self, deltaTime)
    if not self.transform then return end
    
    -- Aktualizacja kąta
    self.currentAngle = self.currentAngle + self.orbitSpeed * deltaTime
    
    -- Zapętlenie kąta (opcjonalne)
    self.currentAngle = self.currentAngle % (2 * math.pi)
    
    UpdateOrbitPosition(self)
end