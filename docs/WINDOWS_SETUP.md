# Windows Development Setup

## Windows Defender / SmartScreen Configuration

When building C++ executables locally, Windows Defender or SmartScreen may block them with:
```
'...\test_policy_validator.exe' was blocked by your organization's Device Guard policy.
```

This is normal for unsigned executables. **Each developer** needs to add a Defender exclusion:

### Option 1: GUI Method (Recommended)

1. Open **Windows Security** (search in Start menu)
2. Go to **Virus & threat protection**
3. Click **Manage settings** under "Virus & threat protection settings"
4. Scroll down and click **Add or remove exclusions**
5. Click **Add an exclusion** → **Folder**
6. Browse to your project's `build` folder:
   ```
   D:\CodeGeass\AIGuardian\build
   ```
7. Click **Select Folder**

### Option 2: PowerShell Method

Run PowerShell **as Administrator**:

```powershell
Add-MpPreference -ExclusionPath "D:\path\to\your\AIGuardian\build"
```

Replace the path with your actual project location.

### Option 3: Per-Developer Signing (Advanced)

If your team prefers code signing, each developer can run:

```powershell
.\scripts\sign-executables.ps1
```

This creates a **local self-signed certificate** and signs all executables. The certificate is NOT shared between developers.

## Verifying the Exclusion

After adding the exclusion, rebuild and test:

```bash
cmake --build build
cd build && ctest --output-on-failure
```

All tests should now run without being blocked.

## Team Workflow

- **DO**: Add the `build/` folder to Defender exclusions in your local environment
- **DON'T**: Commit certificates or try to share signing credentials
- **DOCUMENT**: If you encounter new blocking behavior, update this guide

## Troubleshooting

**Still blocked after adding exclusion?**
- Ensure you added the **folder**, not individual files
- Check that the path matches your actual build directory
- Try temporarily disabling **Real-time protection** to verify it's a Defender issue
- Restart your terminal/IDE after adding exclusions

**Get "Access Denied" when adding exclusions?**
- You need local administrator privileges
- Contact your IT department if on a managed device
