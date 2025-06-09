// VBCableIntegration.cpp - VB-Cable virtual audio device integration
// VB-Cable is handled at the system level, no direct integration needed

#include <string>

namespace quiet {
namespace platform {

bool isVBCableInstalled() {
    // Check if VB-Cable is available in system audio devices
    // This would be done through JUCE's audio device enumeration
    return true; // Placeholder
}

std::string getVBCableInstallInstructions() {
    return "Download and install VB-Cable from:\n"
           "https://vb-audio.com/Cable/\n"
           "Run as administrator and follow the installation wizard.";
}

} // namespace platform
} // namespace quiet