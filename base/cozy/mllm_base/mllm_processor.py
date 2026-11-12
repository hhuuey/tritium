from pathlib import Path
from typing import Any


def _resolve_image_item(content_item: dict[str, Any]) -> Any:
    image_obj = content_item.get('image')
    if image_obj is not None:
        return image_obj

    image_obj = content_item.get('url')
    if image_obj is not None:
        return image_obj

    image_obj = content_item.get('path')
    if image_obj is not None:
        return image_obj

    return None


def _load_image_for_processor(image_obj: Any) -> Any:
    if not isinstance(image_obj, str):
        return image_obj

    image_path = Path(image_obj).expanduser()
    if not image_path.exists():
        return image_obj

    from PIL import Image

    with Image.open(image_path) as img:
        return img.convert('RGB')


def _extract_images_from_messages(messages: list[dict[str, Any]]) -> list[Any]:
    images = []
    for message in messages:
        content = message.get('content', [])
        if not isinstance(content, list):
            continue

        for content_item in content:
            if not isinstance(content_item, dict):
                continue
            if content_item.get('type') != 'image':
                continue

            image_obj = _resolve_image_item(content_item)
            if image_obj is None:
                continue
            images.append(_load_image_for_processor(image_obj))

    return images


def build_multimodal_inputs(
    processor: Any,
    messages_batch: list[list[dict[str, Any]]],
) -> Any:
    try:
        prompt_texts = processor.apply_chat_template(
            messages_batch,
            add_generation_prompt=True,
            tokenize=False,
            enable_thinking=False,
        )
    except TypeError:
        prompt_texts = processor.apply_chat_template(
            messages_batch,
            add_generation_prompt=True,
            tokenize=False,
        )

    images_batch = []
    for messages in messages_batch:
        sample_images = _extract_images_from_messages(messages)
        if not sample_images:
            raise ValueError('all samples must include at least one image for multimodal inference')
        images_batch.append(sample_images)

    return processor(
        text=prompt_texts,
        images=images_batch,
        padding=True,
        return_tensors='pt',
    )


def load_mllm_processor(model_pt_path: str, local_files_only: bool) -> Any:
    from transformers import AutoProcessor

    processor = AutoProcessor.from_pretrained(
        model_pt_path,
        local_files_only=local_files_only,
    )
    tokenizer = getattr(processor, 'tokenizer', None)
    if tokenizer is None:
        return processor

    tokenizer.padding_side = 'left'
    if tokenizer.pad_token_id is not None:
        return processor

    eos_token = getattr(tokenizer, 'eos_token', None)
    if eos_token is None:
        return processor

    tokenizer.pad_token = eos_token
    return processor
