package com.example.rebuilt

import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.min
import kotlin.math.roundToInt

private const val SKELETON_ASSET = "mecha_1004d_ske.json"
private const val ATLAS_ASSET = "mecha_1004d_tex.json"
private const val TEXTURE_ASSET = "mecha_1004d_tex.png"
private const val MAIN_ARMATURE_INDEX = 0
private const val SPRITE_STRIDE = 15
private const val RIGHT_HAND_BONE = "hand_r"

private val BackgroundColor = Color(0xFF0C0D12)
private val PanelColor = Color(0xFF15161F)
private val GridColor = Color(0xFF232533)
private val StrokeColor = Color(0xFF2A2C3A)
private val AccentColor = Color(0xFF7CFFB2)
private val MutedColor = Color(0xFF8E92A6)
private val TextColor = Color(0xFFE8E9F0)

private data class SpriteDraw(
    val source: Rect,
    val destination: RectF,
    val matrix: Matrix,
    val alpha: Int
)

class MainActivity : ComponentActivity() {

    companion object {
        const val EXTRA_TITLE = "feature_title"
        const val EXTRA_BODY = "feature_body"

        init {
            System.loadLibrary("native-lib")
        }
    }

    external fun getEngineVersion(): String
    external fun loadDragonBones(
        assetManager: AssetManager,
        skeletonPath: String,
        atlasPath: String
    ): Boolean
    external fun getLoadError(): String
    external fun getSkeletonName(): String
    external fun getSkeletonFrameRate(): Int
    external fun getArmatureNames(): Array<String>
    external fun getBoneNames(armatureIndex: Int): Array<String>
    external fun getAnimationNames(armatureIndex: Int): Array<String>
    external fun getSlotNames(armatureIndex: Int): Array<String>
    external fun getAtlasImagePath(): String
    external fun getAtlasSize(): IntArray
    external fun getAtlasRegionCount(): Int
    external fun setBoneOffset(boneName: String, x: Float, y: Float)
    external fun clearBoneOffsets()
    external fun getRenderSpriteData(armatureIndex: Int): FloatArray
    external fun getArmatureBounds(armatureIndex: Int): FloatArray

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val loaded = loadDragonBones(assets, SKELETON_ASSET, ATLAS_ASSET)
        val error = getLoadError()
        val skeletonName = if (loaded) getSkeletonName() else ""
        val frameRate = if (loaded) getSkeletonFrameRate() else 0
        val regionCount = if (loaded) getAtlasRegionCount() else 0
        val bones = if (loaded) getBoneNames(MAIN_ARMATURE_INDEX).toList() else emptyList()
        val bounds = if (loaded) getArmatureBounds(MAIN_ARMATURE_INDEX) else floatArrayOf()
        val texture = if (loaded) loadTexture() else null
        val version = getEngineVersion()

        val fetchSprites: () -> FloatArray = {
            if (loaded) getRenderSpriteData(MAIN_ARMATURE_INDEX) else floatArrayOf()
        }
        val applyOffset: (String, Float, Float) -> Unit = { bone, x, y ->
            setBoneOffset(bone, x, y)
        }
        val resetOffsets: () -> Unit = { clearBoneOffsets() }

        setContent {
            CharacterRigStudio(
                loaded = loaded,
                error = error,
                version = version,
                skeletonName = skeletonName,
                frameRate = frameRate,
                regionCount = regionCount,
                bones = bones,
                bounds = bounds,
                texture = texture,
                fetchSprites = fetchSprites,
                applyOffset = applyOffset,
                resetOffsets = resetOffsets
            )
        }
    }

    private fun loadTexture(): Bitmap? {
        return try {
            assets.open(TEXTURE_ASSET).use { stream -> BitmapFactory.decodeStream(stream) }
        } catch (_: Exception) {
            null
        }
    }
}

@Composable
private fun CharacterRigStudio(
    loaded: Boolean,
    error: String,
    version: String,
    skeletonName: String,
    frameRate: Int,
    regionCount: Int,
    bones: List<String>,
    bounds: FloatArray,
    texture: Bitmap?,
    fetchSprites: () -> FloatArray,
    applyOffset: (String, Float, Float) -> Unit,
    resetOffsets: () -> Unit
) {
    val colors = darkColorScheme(
        primary = AccentColor,
        onPrimary = BackgroundColor,
        background = BackgroundColor,
        onBackground = TextColor,
        surface = PanelColor,
        onSurface = TextColor,
        outline = StrokeColor
    )

    val initialBone = remember(bones) {
        if (bones.contains(RIGHT_HAND_BONE)) RIGHT_HAND_BONE else bones.firstOrNull().orEmpty()
    }
    var selectedBone by remember(initialBone) { mutableStateOf(initialBone) }
    var offsetX by remember(initialBone) { mutableFloatStateOf(0f) }
    var offsetY by remember(initialBone) { mutableFloatStateOf(0f) }
    var revision by remember { mutableIntStateOf(0) }
    var menuExpanded by remember { mutableStateOf(false) }

    val sprites = remember(revision, loaded) {
        if (loaded) decodeSprites(fetchSprites()) else emptyList()
    }
    val paint = remember { Paint(Paint.FILTER_BITMAP_FLAG or Paint.ANTI_ALIAS_FLAG) }

    MaterialTheme(colorScheme = colors) {
        Surface(modifier = Modifier.fillMaxSize(), color = BackgroundColor) {
            Row(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(14.dp),
                horizontalArrangement = Arrangement.spacedBy(14.dp)
            ) {
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxHeight()
                ) {
                    Text(
                        text = "CHROMA TOONS",
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextColor
                    )
                    Text(
                        text = if (loaded) {
                            "$skeletonName  /  $frameRate fps  /  $regionCount atlas regions"
                        } else {
                            "DragonBones renderer"
                        },
                        fontSize = 12.sp,
                        color = MutedColor,
                        modifier = Modifier.padding(top = 2.dp, bottom = 10.dp)
                    )

                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(18.dp))
                            .background(PanelColor)
                    ) {
                        Canvas(modifier = Modifier.fillMaxSize()) {
                            val aabbX = bounds.getOrElse(0) { -232.77f }
                            val aabbY = bounds.getOrElse(1) { -275.91f }
                            val worldWidth = bounds.getOrElse(2) { 464.76f }.coerceAtLeast(1f)
                            val worldHeight = bounds.getOrElse(3) { 315.24f }.coerceAtLeast(1f)
                            val padding = 28f
                            val fit = min(
                                (size.width - padding * 2f) / worldWidth,
                                (size.height - padding * 2f) / worldHeight
                            ).coerceAtLeast(0.01f)
                            val originX = size.width / 2f - (aabbX + worldWidth / 2f) * fit
                            val originY = size.height / 2f - (aabbY + worldHeight / 2f) * fit

                            var gridX = -240f
                            while (gridX <= 240f) {
                                val px = originX + gridX * fit
                                if (px >= 0f && px <= size.width) {
                                    drawLine(
                                        color = GridColor,
                                        start = Offset(px, 0f),
                                        end = Offset(px, size.height),
                                        strokeWidth = 1f
                                    )
                                }
                                gridX += 40f
                            }
                            var gridY = -280f
                            while (gridY <= 80f) {
                                val py = originY + gridY * fit
                                if (py >= 0f && py <= size.height) {
                                    drawLine(
                                        color = GridColor,
                                        start = Offset(0f, py),
                                        end = Offset(size.width, py),
                                        strokeWidth = 1f
                                    )
                                }
                                gridY += 40f
                            }

                            if (texture != null && sprites.isNotEmpty()) {
                                drawIntoCanvas { canvas ->
                                    val native = canvas.nativeCanvas
                                    native.save()
                                    native.translate(originX, originY)
                                    native.scale(fit, fit)
                                    for (sprite in sprites) {
                                        native.save()
                                        native.concat(sprite.matrix)
                                        paint.alpha = sprite.alpha
                                        native.drawBitmap(
                                            texture,
                                            sprite.source,
                                            sprite.destination,
                                            paint
                                        )
                                        native.restore()
                                    }
                                    native.restore()
                                }
                            }
                        }

                        if (!loaded || texture == null) {
                            Text(
                                text = if (!loaded) "Load failed: $error" else "Texture decode failed",
                                color = Color(0xFFFF8A8A),
                                fontSize = 14.sp,
                                modifier = Modifier
                                    .align(Alignment.Center)
                                    .padding(24.dp)
                            )
                        }

                        Row(
                            modifier = Modifier
                                .align(Alignment.BottomStart)
                                .fillMaxWidth()
                                .padding(12.dp),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            Text(
                                text = "${sprites.size} sprites",
                                fontSize = 11.sp,
                                color = MutedColor,
                                fontFamily = FontFamily.Monospace
                            )
                            Text(
                                text = String.format(
                                    "%.1f, %.1f  %.1f x %.1f",
                                    bounds.getOrElse(0) { 0f },
                                    bounds.getOrElse(1) { 0f },
                                    bounds.getOrElse(2) { 0f },
                                    bounds.getOrElse(3) { 0f }
                                ),
                                fontSize = 11.sp,
                                color = MutedColor,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }
                }

                Column(
                    modifier = Modifier
                        .width(286.dp)
                        .fillMaxHeight()
                        .clip(RoundedCornerShape(18.dp))
                        .background(PanelColor)
                        .verticalScroll(rememberScrollState())
                        .padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Text(
                        text = "LIVE RIG",
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextColor
                    )
                    Text(
                        text = "Offset the selected bone in local space. Hand_R is selected by default.",
                        fontSize = 11.sp,
                        color = MutedColor,
                        lineHeight = 16.sp
                    )

                    Box(modifier = Modifier.fillMaxWidth()) {
                        OutlinedButton(
                            onClick = { menuExpanded = true },
                            enabled = loaded && bones.isNotEmpty(),
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                text = selectedBone.ifEmpty { "No bones" },
                                modifier = Modifier.weight(1f),
                                maxLines = 1,
                                fontSize = 12.sp,
                                fontFamily = FontFamily.Monospace
                            )
                            Text(text = "v", fontSize = 11.sp)
                        }
                        DropdownMenu(
                            expanded = menuExpanded,
                            onDismissRequest = { menuExpanded = false },
                            modifier = Modifier.heightIn(max = 340.dp)
                        ) {
                            for (bone in bones) {
                                DropdownMenuItem(
                                    text = {
                                        Text(
                                            text = bone,
                                            fontSize = 12.sp,
                                            fontFamily = FontFamily.Monospace
                                        )
                                    },
                                    onClick = {
                                        selectedBone = bone
                                        offsetX = 0f
                                        offsetY = 0f
                                        resetOffsets()
                                        revision++
                                        menuExpanded = false
                                    }
                                )
                            }
                        }
                    }

                    AxisSlider(
                        label = "${boneLabel(selectedBone)} X",
                        value = offsetX,
                        enabled = loaded && selectedBone.isNotEmpty(),
                        onValueChange = { value ->
                            offsetX = value
                            applyOffset(selectedBone, offsetX, offsetY)
                            revision++
                        }
                    )
                    AxisSlider(
                        label = "${boneLabel(selectedBone)} Y",
                        value = offsetY,
                        enabled = loaded && selectedBone.isNotEmpty(),
                        onValueChange = { value ->
                            offsetY = value
                            applyOffset(selectedBone, offsetX, offsetY)
                            revision++
                        }
                    )

                    OutlinedButton(
                        onClick = {
                            offsetX = 0f
                            offsetY = 0f
                            resetOffsets()
                            revision++
                        },
                        enabled = loaded,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(text = "Reset rig", fontSize = 12.sp)
                    }

                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(1.dp)
                            .background(StrokeColor)
                    )

                    Text(
                        text = "ENGINE",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        color = MutedColor,
                        letterSpacing = 1.sp
                    )
                    PanelLine("Version", version)
                    PanelLine("Skeleton", skeletonName.ifEmpty { "-" })
                    PanelLine("Frame rate", if (frameRate > 0) "$frameRate fps" else "-")
                    PanelLine("Atlas regions", regionCount.toString())
                    PanelLine("Bones", bones.size.toString())
                    PanelLine("Selected", selectedBone.ifEmpty { "-" })

                    Spacer(modifier = Modifier.height(2.dp))
                    Text(
                        text = "Native C++ computes bone matrices, slot pivots and the recursive weapon armature.",
                        fontSize = 10.sp,
                        color = MutedColor,
                        lineHeight = 15.sp
                    )
                }
            }
        }
    }
}

@Composable
private fun AxisSlider(
    label: String,
    value: Float,
    enabled: Boolean,
    onValueChange: (Float) -> Unit
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(text = label, fontSize = 12.sp, color = TextColor)
            Text(
                text = value.roundToInt().toString(),
                fontSize = 12.sp,
                color = AccentColor,
                fontFamily = FontFamily.Monospace
            )
        }
        Slider(
            value = value,
            onValueChange = onValueChange,
            enabled = enabled,
            valueRange = -80f..80f,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

@Composable
private fun PanelLine(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(text = label, fontSize = 11.sp, color = MutedColor)
        Text(
            text = value,
            fontSize = 11.sp,
            color = TextColor,
            fontFamily = FontFamily.Monospace
        )
    }
}

private fun boneLabel(boneName: String): String {
    return when (boneName) {
        "hand_r" -> "Right Hand"
        "hand_l" -> "Left Hand"
        "root" -> "Root"
        "root1" -> "Root 1"
        "pelvis" -> "Pelvis"
        "chest" -> "Chest"
        "head" -> "Head"
        else -> boneName
    }
}

private fun decodeSprites(data: FloatArray): List<SpriteDraw> {
    if (data.size < SPRITE_STRIDE) {
        return emptyList()
    }
    val result = ArrayList<SpriteDraw>(data.size / SPRITE_STRIDE)
    var index = 0
    while (index + SPRITE_STRIDE <= data.size) {
        val atlasX = data[index].toInt()
        val atlasY = data[index + 1].toInt()
        val atlasWidth = data[index + 2].toInt()
        val atlasHeight = data[index + 3].toInt()
        val x = data[index + 4]
        val y = data[index + 5]
        val width = data[index + 6]
        val height = data[index + 7]
        val a = data[index + 8]
        val b = data[index + 9]
        val c = data[index + 10]
        val d = data[index + 11]
        val tx = data[index + 12]
        val ty = data[index + 13]
        val alpha = (data[index + 14] * 255f).roundToInt().coerceIn(0, 255)

        val matrix = Matrix()
        matrix.setValues(floatArrayOf(a, c, tx, b, d, ty, 0f, 0f, 1f))
        result.add(
            SpriteDraw(
                source = Rect(atlasX, atlasY, atlasX + atlasWidth, atlasY + atlasHeight),
                destination = RectF(x, y, x + width, y + height),
                matrix = matrix,
                alpha = alpha
            )
        )
        index += SPRITE_STRIDE
    }
    return result
}
