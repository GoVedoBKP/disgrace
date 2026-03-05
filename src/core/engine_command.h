namespace disgrace_ns
{

enum class EngineCommandType
{
    Play,
    Stop,
    SetTempo,
    PlayPattern
};

struct EngineCommand
{
    EngineCommandType type;
    float value;
};

} // namespace disgrace_ns
