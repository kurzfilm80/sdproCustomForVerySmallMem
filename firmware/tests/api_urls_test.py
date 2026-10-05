"""Exercise the exact API format strings through native snprintf."""
import ast
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="sdpro-url-") as temporary:
    directory = Path(temporary)
    for source, variable, capacity, path in [
        ("SeoulWeather.cpp", "kWeatherUrl", 320, "/v1/forecast"),
        ("SeoulAirQuality.cpp", "kAirQualityUrl", 240, "/v1/air-quality"),
    ]:
        text = (root / "src" / source).read_text()
        declaration = re.search(r"\b" + variable + r"\[\]\s*=\s*(.*?);", text, re.S).group(1)
        literals = re.findall(r'"(?:[^"\\]|\\.)*"', declaration)
        url = "".join(ast.literal_eval(literal) for literal in literals)
        code = '#include <cstdio>\nint main() { char url[' + str(capacity) + '];\n'
        code += 'int n = snprintf(url, sizeof(url), ' + declaration + ', "-90.000000", "-180.000000");\n'
        code += 'if (n < 0 || n >= sizeof(url)) return 1; puts(url); }\n'
        file = directory / "url.cpp"
        file.write_text(code)
        binary = directory / "url"
        subprocess.run(["c++", "-Wall", "-Werror", str(file), "-o", str(binary)], check=True)
        result = subprocess.check_output([str(binary)], text=True).strip()
        assert path + "?latitude=-90.000000&longitude=-180.000000&" in result
        assert "&timezone=Asia%2FSeoul&forecast_days=3" in result
        assert "37.5665" not in result
print("PASS: both actual API templates, custom coordinates, KST escaping and URL bounds")
