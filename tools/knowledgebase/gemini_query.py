import os
import json
import argparse
import http.client
import sys
from datetime import datetime

def load_config():
    config_path = r'C:\key\config.json'
    if not os.path.exists(config_path):
        print(f"Error: {config_path} not found. Please create it with api_key and model fields.")
        sys.exit(1)
    with open(config_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def call_gemini(api_key, model, prompt, system=None, temperature=None, max_tokens=None):
    host = "generativelanguage.googleapis.com"
    endpoint = f"/v1beta/models/{model}:generateContent?key={api_key}"

    payload = {
        "contents": [{
            "parts": [{"text": prompt}]
        }]
    }
    if system:
        payload["systemInstruction"] = {"parts": [{"text": system}]}
    gen = {}
    if temperature is not None:
        gen["temperature"] = temperature
    if max_tokens is not None:
        gen["maxOutputTokens"] = max_tokens
    if gen:
        payload["generationConfig"] = gen

    headers = {"Content-Type": "application/json"}

    conn = http.client.HTTPSConnection(host, timeout=600)
    conn.request("POST", endpoint, body=json.dumps(payload), headers=headers)

    response = conn.getresponse()
    data = response.read()
    conn.close()

    if response.status != 200:
        print(f"API Error: HTTP {response.status}")
        print(data.decode('utf-8'))
        sys.exit(1)

    result = json.loads(data.decode('utf-8'))
    try:
        cand = result['candidates'][0]
        parts = cand.get('content', {}).get('parts', [])
        text = "".join(p.get('text', '') for p in parts if 'text' in p)
        if not text:
            fr = cand.get('finishReason', 'UNKNOWN')
            print(f"Empty response (finishReason={fr}).")
            print(json.dumps(cand, ensure_ascii=False)[:2000])
            sys.exit(1)
        return text
    except (KeyError, IndexError):
        print("Unexpected response format.")
        print(json.dumps(result, ensure_ascii=False)[:2000])
        sys.exit(1)

def list_models(api_key):
    host = "generativelanguage.googleapis.com"
    endpoint = f"/v1beta/models?key={api_key}"
    
    conn = http.client.HTTPSConnection(host)
    conn.request("GET", endpoint)
    
    response = conn.getresponse()
    data = response.read()
    conn.close()
    
    if response.status != 200:
        print(f"API Error: HTTP {response.status}")
        sys.exit(1)
        
    result = json.loads(data.decode('utf-8'))
    models = []
    for m in result.get('models', []):
        name = m.get('name', '').replace('models/', '')
        desc = m.get('description', '')
        models.append(f"{name} : {desc}")
    
    list_file = os.path.join(os.path.dirname(__file__), 'models_list.txt')
    with open(list_file, 'w', encoding='utf-8') as f:
        f.write("\n".join(models))
    
    print(f"Successfully updated model list in: {list_file}")
    for m in models:
        print(f" - {m.split(' : ')[0]}")

def main():
    parser = argparse.ArgumentParser(description="Query Gemini API")
    parser.add_argument("prompt", nargs='?', help="The question or prompt to ask Gemini")
    parser.add_argument("-f", "--prompt-file", help="Read the prompt from a UTF-8 file (avoids argv length/encoding limits). If a positional prompt is also given, it is appended after the file content.")
    parser.add_argument("-o", "--output", help="Save output to a text file")
    parser.add_argument("-l", "--list-models", action="store_true", help="List available models and save to models_list.txt")
    parser.add_argument("-m", "--model", help="Override the model from config.json (e.g. gemini-2.5-pro)")
    parser.add_argument("--system", help="Optional system instruction (role/context)")
    parser.add_argument("--system-file", help="Read the system instruction from a UTF-8 file")
    parser.add_argument("--temperature", type=float, help="Sampling temperature, 0-2 (omitted = model default)")
    parser.add_argument("--max-tokens", type=int, help="maxOutputTokens cap (omitted = model default/max)")

    args = parser.parse_args()
    if args.system_file:
        with open(args.system_file, 'r', encoding='utf-8') as f:
            args.system = f.read()

    if args.prompt_file:
        with open(args.prompt_file, 'r', encoding='utf-8') as f:
            file_prompt = f.read()
        args.prompt = (file_prompt + ("\n\n" + args.prompt if args.prompt else "")) if file_prompt.strip() else args.prompt
    
    config = load_config()
    api_key = config.get("api_key")
    
    if not api_key:
        print("Error: API key is missing in config.json.")
        sys.exit(1)

    if args.list_models:
        list_models(api_key)
        return

    if not args.prompt:
        parser.print_help()
        return
        
    model = args.model or config.get("model", "gemini-1.5-flash")
    print(f"Querying Gemini ({model})...")
    answer = call_gemini(api_key, model, args.prompt, system=args.system,
                         temperature=args.temperature, max_tokens=args.max_tokens)

    # Log every query + response to message/ with a timestamped filename
    msg_dir = os.path.join(os.path.dirname(__file__), 'message')
    os.makedirs(msg_dir, exist_ok=True)
    stamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    log_path = os.path.join(msg_dir, f'{stamp}.txt')
    with open(log_path, 'w', encoding='utf-8') as f:
        f.write(f"Timestamp: {datetime.now().isoformat(timespec='seconds')}\n")
        f.write(f"Model:     {model}\n")
        f.write("=" * 72 + "\n")
        f.write("QUESTION\n")
        f.write("=" * 72 + "\n")
        f.write(args.prompt.rstrip() + "\n\n")
        f.write("=" * 72 + "\n")
        f.write("RESPONSE\n")
        f.write("=" * 72 + "\n")
        f.write(answer.rstrip() + "\n")
    print(f"Logged to: {log_path}")

    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(answer)
        print(f"Response saved to: {args.output}")
    else:
        print("\n--- Response ---\n")
        print(answer)

if __name__ == "__main__":
    main()
