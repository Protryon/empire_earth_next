// TEST ONLY: EE-SIM.exe's entry (build.py --sim links it in place of nothing: EE-AOC.exe's objects plus this one, as a
// console program). The headless simulation itself is EESim.cpp (platform/PlatSim.h).
int EESimMain(int argc, char** argv);

int main(int argc, char** argv)
{
    return EESimMain(argc, argv);
}
