-- DebugScript.lua
-- Simplified script with more debug output

-- Print directly to stdout
print("Script file loaded")

-- Variables for tracking state
local lastPrintTime = 0

-- OnCreate now receives the script table as 'self'
function OnCreate(self)
    print("OnCreate called for entity " .. self.entityId)
    
    -- Try both logging methods
    print("Logging via print()")
    
    -- Try using Logger if available
    if Logger then
        print("Logger table exists")
        if Logger.info then
            Logger.info("DebugScript: OnCreate called via Logger.info")
        else
            print("Logger.info function not found")
        end
    else
        print("Logger table not found")
    end
    
    -- Test Engine binding
    if Engine then
        print("Engine table exists")
        if Engine.test then
            local result = Engine.test()
            print("Engine.test result: " .. result)
        else
            print("Engine.test function not found")
        end
        
        if Engine.getTotalTime then
            print("Current engine time: " .. Engine.getTotalTime())
        elseif Engine.getTime then
            print("Current engine time: " .. Engine.getTime())
        else
            print("Engine time function not found")
        end
    else
        print("Engine table not found")
    end
    
    -- Store a reference to self in the global environment for direct access
    -- This is optional but can be useful
    this = self
end

-- OnUpdate now receives the script table as 'self' and then deltaTime
function OnUpdate(self, deltaTime)
    -- Only print occasionally to avoid console spam
    local currentTime = 0
    if Engine and Engine.getTotalTime then
        currentTime = Engine.getTotalTime()
    elseif Engine and Engine.getTime then
        currentTime = Engine.getTime()
    end
    
    if currentTime - lastPrintTime >= 1.0 then
        print("OnUpdate called for entity " .. self.entityId .. ", deltaTime = " .. deltaTime)
        lastPrintTime = currentTime
        
        -- Try Logger if available
        if Logger and Logger.info then
            Logger.info("DebugScript: OnUpdate via Logger")
        end
    end
end

-- OnDestroy now receives the script table as 'self'
function OnDestroy(self)
    print("OnDestroy called for entity " .. self.entityId)
    
    -- Try Logger if available
    if Logger and Logger.info then
        Logger.info("DebugScript: OnDestroy via Logger")
    end
end

-- Print again at end of file to verify complete loading
print("Script file fully parsed")