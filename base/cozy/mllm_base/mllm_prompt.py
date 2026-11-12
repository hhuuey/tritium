import json


def load_prompt_text(prompt_path: str) -> str:
    with open(prompt_path, 'r', encoding='utf-8') as file:
        return file.read()


def load_prompt_pair(system_prompt_path: str, user_prompt_path: str) -> tuple[str, str]:
    return load_prompt_text(system_prompt_path), load_prompt_text(user_prompt_path)


def generate_feedback_messages(response: str, feedback: str) -> list[dict[str, str | list[dict[str, str]]]]:
    response_message = {
        'role': 'assistant',
        'content': [{
            'type': 'text',
            'text': response,
        }],
    }
    feedback_message = {
        'role': 'user',
        'content': [{
            'type': 'text',
            'text': feedback,
        }],
    }
    return [response_message, feedback_message]


def build_assistant_message(payload: dict) -> dict[str, str]:
    return {
        'role': 'assistant',
        'content': json.dumps(payload, ensure_ascii=False),
    }
