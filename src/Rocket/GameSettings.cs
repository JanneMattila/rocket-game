using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Rocket;

public static class GameSettings
{
    public static int ScreenWidth { get; set; } = 1024;
    public static int ScreenHeight { get; set; } = 768;

    public static float NetworkUpdateTime { get; set; } = 1f / 30f;
}
