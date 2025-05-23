# PDACPIPlatform

Work in Progress [WIP] Project, PDACPIPlatform is a kernel extension (kext) for the PureDarwin operating system. It serves as an open-source alternative to Apple's proprietary `AppleACPIPlatform.kext`.

The primary purpose of this kext is to initialize and manage the ACPI (Advanced Configuration and Power Interface) subsystem by leveraging Intel's ACPICA (ACPI Component Architecture) library. This allows for more flexible and potentially improved ACPI functionality on a wider range of hardware, particularly for systems running PureDarwin on various hardware.

## What it does

PDACPIPlatform integrates Intel's ACPICA library to provide core ACPI functionalities. This includes:

*   **ACPICA Initialization:** Sets up and initializes the ACPICA environment within the PureDarwin kernel.
*   **ACPI Table Loading:** Loads and enables ACPI tables necessary for system operation.
*   **Power Management:** Facilitates proper power management events, such as sleep (S3), wake, and shutdown (S5).
*   **Hardware Configuration:** Assists in the ACPI-based configuration and enumeration of hardware devices.
*   **Object Evaluation:** Allows for the evaluation of ACPI objects defined in the system's DSDT/SSDTs.

By providing these capabilities, PDACPIPlatform aims to improve compatibility and stability for PureDarwin on various hardware configurations.

## Building the Project

This project is configured to be built using Xcode.

1.  **Clone the repository:**
    ```bash
    git clone <repository-url>
    cd PDACPIPlatform
    ```
2.  **Open the project in Xcode:**
    Open `PDACPIPlatform.xcodeproj` with Xcode.
3.  **Select the build target:**
    Choose the `PDACPIPlatform` target. The build settings should be configured for a generic Darwin target, suitable for PureDarwin. You may need to adjust architecture settings based on your PureDarwin environment.
4.  **Build the project:**
    Click the "Build" button (Play icon) or select Product > Build from the menu.
5.  **Locate the kext:**
    The compiled `PDACPIPlatform.kext` will be located in the "Products" directory (usually under `Build/Products/Debug` or `Build/Products/Release`).

**Note:** You may need to have Xcode Command Line Tools installed. Ensure your Xcode version is compatible with the project's settings.

## Installation

**Note:** The following are general guidelines for installing a kernel extension (kext) on PureDarwin. Please consult the official PureDarwin documentation for the most accurate and up-to-date installation procedures.

**Warning:** Installing kernel extensions can be risky and may lead to system instability if not done correctly. Proceed with caution and ensure you have a backup of your system or a way to recover.

1.  **Build the Kext:**
    *   Follow the "Building the Project" section to compile `PDACPIPlatform.kext`.

2.  **Transfer the Kext to your PureDarwin System:**
    *   You'll need to transfer the `PDACPIPlatform.kext` bundle to your PureDarwin environment. The method will depend on how you are running PureDarwin (e.g., physical machine, virtual machine).

3.  **Install the Kext:**
    *   Kernel extensions in PureDarwin are typically installed in a specific directory, often `/System/Library/Extensions/` or a similar path.
    *   Copy `PDACPIPlatform.kext` to the appropriate kext directory on your PureDarwin system. You may need superuser privileges.
        ```bash
        # Example:
        sudo cp -R PDACPIPlatform.kext /System/Library/Extensions/
        ```
    *   Ensure the kext has the correct ownership and permissions. Typically, this would be `root:wheel` and `755` for directories and `644` for files within the kext bundle.
        ```bash
        # Example:
        sudo chown -R root:wheel /System/Library/Extensions/PDACPIPlatform.kext
        sudo chmod -R 755 /System/Library/Extensions/PDACPIPlatform.kext
        # You might need to adjust permissions for files inside the kext bundle more granularly.
        ```

4.  **Update Kext Cache/Dependencies (if applicable):**
    *   PureDarwin may have a mechanism similar to `kextcache` on macOS to rebuild a cache of kext information or update dependencies. Consult PureDarwin documentation for commands like `kextload` or any kext management utilities.

5.  **Reboot:**
    *   A reboot is typically required for the system to load the new kernel extension.

**Important Considerations:**

*   **System Compatibility:** Ensure this kext is compatible with your version of PureDarwin.
*   **Dependencies:** Based on the project's `Info.plist`, this kext depends on `IOACPIFamily` and `IOPCIFamily`. These should be present in your PureDarwin system.
*   **Conflicts:** Remove or disable any other conflicting ACPI platform kexts to avoid issues.
*   **Debugging:** If you encounter issues, check system logs for messages related to `PDACPIPlatform` during boot and operation. PureDarwin's debugging tools and bootloader options might provide more insight.

## License

This project is licensed under the terms found in the `LICENSE.txt` file at the root of this repository. This license pertains to the PDACPIPlatform kext code itself.

PDACPIPlatform incorporates the ACPI Component Architecture (ACPICA) library developed by Intel. The ACPICA library has its own licensing terms, which can be found in the `PDACPIPlatform/ACPICA_LICENSE` file.

By using, distributing, or contributing to this project, you agree to comply with the terms of both licenses.

## Credits

*   **The PureDarwin Project:** This kext originated as part of the PureDarwin project, which aims to create a community-driven open-source operating system based on Apple's Darwin source code.
*   **github.com/csekel (InSaneDarwin):** Credited as the original creator of PDACPIPlatform, providing an open-source version of Apple's AppleACPIPlatform.
*   **Intel Corporation:** For developing and maintaining the ACPICA (ACPI Component Architecture) library, which is a core component of this kext.

## Disclaimer and Warning

This software is provided "AS IS" without any express or implied warranties, including but not limited to the implied warranties of merchantability and fitness for a particular purpose.

Kernel extensions (kexts) operate at a low level of the operating system. Incorrect use, bugs in the software, or hardware incompatibilities can potentially lead to system instability, data loss, or even hardware issues in extreme cases.

**By downloading, installing, or using PDACPIPlatform, you acknowledge that you understand the risks involved and agree that the developers and contributors are not liable for any damages that may occur.**

It is strongly recommended to:
*   Back up your system before installing any new kernel extension.
*   Understand what the kext does before installing it.
*   Verify compatibility with your hardware and PureDarwin version.
